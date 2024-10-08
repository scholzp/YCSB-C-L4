/* Benchmark server using only IPC for communication.
 *
 * Author: Viktor Reusch
 */

#include <iostream>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/err.h>
#include <l4/sys/factory>
#include <l4/sys/ipc_gate>
#include <l4/sys/scheduler>
#include <pthread-l4.h>

#include "db.h"
#include "serializer.h"
#include "sqlite_ipc_server.h" // IPC interface for this server
#include "sqlite_lib_db.h"
#include "utils.h"

using serializer::Deserializer;
using serializer::Serializer;
using ycsbc::DB;

namespace sqlite {
namespace ipc {

// Server object for the main server (not the worker threads)
Registry main_server;

// Arguments for the BenchServer::loop() function.
struct BenchArgs {
  // Back channel to the caller. Will receive an IPC message when gate is set.
  L4::Cap<L4::Thread> caller;
  L4::Cap<L4Re::Dataspace> in;
  L4::Cap<L4Re::Dataspace> out;
  // Location to return the create gate to.
  L4::Cap<BenchI> *gate;
  ycsbc::SqliteLibDB *db;
  l4_umword_t cpu;
};

// Implements a single benchmark thread, which performs the Read(), Scan(), etc.
// operations.
class BenchServer : public L4::Epiface_t<BenchServer, BenchI> {
  // Registry of this thread. Handles the server loop.
  // The default constructor must not be used from a non-main thread.
  Registry registry{Pthread::L4::cap(pthread_self()),
                    L4Re::Env::env()->factory()};

  // Dataspaces received from client for input and output respectively
  L4::Cap<L4Re::Dataspace> ds_in;
  char *ds_in_addr = 0;

  L4::Cap<L4Re::Dataspace> ds_out;
  char *ds_out_addr = 0;

  // SqliteLibDB object create in the main thread
  ycsbc::SqliteLibDB *database;

  // Context object returned from SqliteLibDB object
  void *sqlite_ctx;

public:
  BenchServer(L4::Cap<L4Re::Dataspace> in, L4::Cap<L4Re::Dataspace> out,
              ycsbc::SqliteLibDB *db) {
    ds_in = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    L4Re::chkcap(ds_in);

    ds_out = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    L4Re::chkcap(ds_out);

    // Move input capabilities to local cap slots
    ds_in.move(in);
    ds_out.move(out);

    database = db;

    // Attach memory windows to this AS
    // Map new dataspaces into this AS
    if (L4Re::Env::env()->rm()->attach(&ds_in_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::RW,
                                       L4::Ipc::make_cap_full(ds_in)) < 0) {
      throw std::runtime_error{"Failed to attach db_in dataspace."};
    }
    if (L4Re::Env::env()->rm()->attach(&ds_out_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::RW,
                                       L4::Ipc::make_cap_full(ds_out)) < 0) {
      throw std::runtime_error{"Failed to attach db_out dataspace."};
    }

    sqlite_ctx = database->Init();
  }

  // Create a new benchmark server running its own server loop on this thread.
  static void *loop(void *void_args) {
    auto args = reinterpret_cast<BenchArgs *>(void_args);

    // Copy arguments out of args because args is not valid after calling
    // l4_ipc_send.
    auto caller = args->caller;
    auto in = args->in;
    auto out = args->out;
    auto gate = args->gate;
    auto db = args->db;
    auto cpu = args->cpu;

    ycsbc::migrate(cpu);

    // FIXME: server is never freed.
    auto server = new BenchServer{in, out, db};
    // FIXME: Capability is never unregistered.
    L4Re::chkcap(server->registry.registry()->register_obj(server));

    *gate = server->obj_cap();

    // Signal that gate is now set.
    auto tag = l4_ipc_send(caller.cap(), l4_utcb(), l4_msgtag(0, 0, 0, 0),
                           L4_IPC_NEVER);
    if (l4_msgtag_has_error(tag))
      throw std::runtime_error{"failed to send signal to caller"};

    // Start waiting for communication.
    server->registry.loop();

    return nullptr;
  }

  // Read some value from the database
  long op_read(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<std::string> fields = std::vector<std::string>(0);

    // Output vector, sent back to client after operation
    std::vector<DB::KVPair> result;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> fields;

    if (database->Read(sqlite_ctx, table, key, &fields, result) != DB::kOK) {
      return (-L4_EINVAL);
    }

    // Put result into output dataspace
    memset(ds_out_addr, '\0', YCSBC_DS_SIZE);
    Serializer s{ds_out_addr, YCSBC_DS_SIZE};
    s << result;

    return (L4_EOK);
  }

  // Scan for some values from the database
  long op_scan(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    int len = 0;
    std::vector<std::string> fields = std::vector<std::string>(0);

    // Output vector, sent back to client after operation
    std::vector<std::vector<DB::KVPair>> result;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> len;
    d >> fields;

    if (database->Scan(sqlite_ctx, table, key, len, &fields, result) !=
        DB::kOK) {
      return (-L4_EINVAL);
    }

    // Put result into output dataspace
    memset(ds_out_addr, '\0', YCSBC_DS_SIZE);
    Serializer s{ds_out_addr, YCSBC_DS_SIZE};
    s << result;

    return (L4_EOK);
  }

  // Insert a value into the database
  long op_insert(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<DB::KVPair> values;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> values;

    if (database->Insert(sqlite_ctx, table, key, values) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Update a value in the database
  long op_update(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<DB::KVPair> values;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> values;

    if (database->Update(sqlite_ctx, table, key, values) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Deletes a value from the database
  long op_del(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;

    if (database->Delete(sqlite_ctx, table, key) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Unmaps the client-provided memory windows
  long op_close(BenchI::Rights) {
    // Detach client mappings
    if (L4Re::Env::env()->rm()->detach(ds_in_addr, &ds_in) < 0) {
      std::cerr << "Failed to detach input dataspace." << std::endl;
      return (-L4_EINVAL);
    }
    if (L4Re::Env::env()->rm()->detach(ds_out_addr, &ds_out) < 0) {
      std::cerr << "Failed to detach output dataspace." << std::endl;
      return (-L4_EINVAL);
    }

    // Free the caps associated with the memory mappings
    L4Re::Util::cap_alloc.free(ds_in);
    L4Re::Util::cap_alloc.free(ds_out);

    return (L4_EOK);
  }

  // Terminates this benchmark thread
  long op_terminate(BenchI::Rights) {
    pthread_exit(NULL);

    // Never reached
    return (L4_EOK);
  }
};

// Implements the interface for the database management and a factory for new
// benchmark threads.
class DbServer : public L4::Epiface_t<DbServer, DbI> {
  // YCSB SQLite backend which we are testing against.
  // TODO: Delete when tearing down this object
  ycsbc::SqliteLibDB *db = nullptr;

public:
  long op_schema(DbI::Rights, L4::Ipc::Snd_fpage buf_cap) {
    // At first, check if we actually received a capability
    if (!buf_cap.cap_received()) {
      std::cerr << "Received fpage was not a capability." << std::endl;
      return (-L4_EACCESS);
    }

    // Now, map the buffer capability to our infopage (index 0, because we
    // only expect one capability to be sent)
    infopage = main_server.rcv_cap<L4Re::Dataspace>(0);
    if (L4Re::Env::env()->rm()->attach(&infopage_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::R,
                                       L4::Ipc::make_cap_full(infopage)) < 0) {
      std::cerr << "Failed to map client-provided infopage.";
      return (-L4_EINVAL);
    }

    Deserializer d{infopage_addr};

    std::string fname{};
    d >> fname;
    db = new ycsbc::SqliteLibDB(fname);

    DB::Tables tables{};

    d >> tables;
    db->CreateSchema(tables);

    return L4_EOK;
  }

  long op_spawn(DbI::Rights, L4::Ipc::Snd_fpage in_buf,
                L4::Ipc::Snd_fpage out_buf, L4::Ipc::Cap<BenchI> &res,
                l4_umword_t cpu) {
    L4::Cap<BenchI> gate;

    // Check if we actually received capabilities
    if (!in_buf.cap_received() || !out_buf.cap_received()) {
      std::cerr << "Received fpages were not capabilities." << std::endl;
      return (-L4_EACCESS);
    }

    // Construct the memory buffer caps from the input arguments
    L4::Cap<L4Re::Dataspace> in = main_server.rcv_cap<L4Re::Dataspace>(0);
    L4::Cap<L4Re::Dataspace> out = main_server.rcv_cap<L4Re::Dataspace>(1);

    pthread_t thread;
    BenchArgs args{
        .caller = Pthread::L4::cap(pthread_self()),
        .in = in,
        .out = out,
        .gate = &gate,
        .db = db,
        .cpu = cpu,
    };
    if (pthread_create(&thread, nullptr, BenchServer::loop, &args))
      throw std::runtime_error{"pthread_create failed"};

    // Wait for other thread to set gate.
    auto tag = l4_ipc_receive(pthread_l4_cap(thread), l4_utcb(), L4_IPC_NEVER);
    if (l4_msgtag_has_error(tag))
      throw std::runtime_error{"receiving from created thread failed"};

    // Return the IPC gate to the benchmark server.
    res = L4::Ipc::make_cap_rw(gate);

    return L4_EOK;
  };

private:
  // Dataspace and address for transferring metadata (such as table layout)
  // from the client to the server
  L4::Cap<L4Re::Dataspace> infopage;
  char *infopage_addr;
};

static void registerServer(Registry &registry) {
  static DbServer server;

  // Register server
  if (!registry.registry()->register_obj(&server, "ipc").is_valid())
    throw std::runtime_error{
        "Could not register IPC server, is there an 'ipc' in the caps table?"};
}

} // namespace ipc
} // namespace sqlite

int main() {
  std::cout << "SQLite 3 Version: " << SQLITE_VERSION << std::endl;

  sqlite::ipc::registerServer(sqlite::ipc::main_server);
  std::cout << "Servers registered. Waiting for requests..." << std::endl;
  sqlite::ipc::main_server.loop();

  return (0);
}
