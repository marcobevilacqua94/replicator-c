#include <cbl/CouchbaseLite.h>
#include <time.h>
#include <inttypes.h>
#ifdef _MSC_VER
#include <direct.h>
#include <Shlwapi.h>
#pragma warning(disable : 4996)
void usleep(unsigned int us) {
    Sleep(us / 1000);
}
#else
#include <unistd.h>
#endif

static CBLDatabase* kDatabase;

// Helper for stop replicator in the code snippet
static void stop_replicator(CBLReplicator* replicator) {
    CBLReplicator_Stop(replicator);
    while (CBLReplicator_Status(replicator).activity != kCBLReplicatorStopped) {
        printf("Waiting for replicator to stop...");
        usleep(2000000);
    }
    CBLReplicator_Release(replicator);
}



static void create_new_database() {
    CBLError err;
    CBLDatabase* db = CBLDatabase_Open(FLSTR("my-databaseNuovo"), NULL, &err);
    // end::new-database[]
    kDatabase = db;


}

static void create_document() {
    CBLDatabase* db = kDatabase;
    CBLDocument* mutableDoc = CBLDocument_Create();
    FLMutableDict properties = CBLDocument_MutableProperties(mutableDoc);
    FLMutableDict_SetFloat(properties, FLSTR("version"), 3.0f);
    CBLError err;
    if (!CBLDatabase_SaveDocument(kDatabase, mutableDoc, &err)) {
        // Failed to save, do error handling as above

        return 1;
    }
    else {
        printf("\nDoc saved");
    }
    FLStringResult id = FLSlice_Copy(CBLDocument_ID(mutableDoc));
    CBLDocument_Release(mutableDoc);


    mutableDoc = CBLDatabase_GetMutableDocument(db, FLSliceResult_AsSlice(id), &err);
    if (!mutableDoc) {
        // Failed to retrieve, do error handling as above.  NOTE: error code 0 simply means
        // the document does not exist.
        return 1;
    }
    properties = CBLDocument_MutableProperties(mutableDoc);
    //TODO set the right channel
    FLMutableDict_SetString(properties, FLSTR("language"), FLSTR("C"));
    if (!CBLDatabase_SaveDocument(db, mutableDoc, &err)) {
        // Failed to save, do error handling as above
        return 1;
    }
}

static void close_database() {
    CBLDatabase* db = kDatabase;
    // tag::close-database[]
    // NOTE: No error handling, for brevity (see getting started)

    CBLError err;
    CBLDatabase_Close(db, &err);
    // end::close-database[]
}




static void getting_started_change_listener(void* context, CBLReplicator* repl, const CBLReplicatorStatus* status) {
    if (status->error.code != 0) {
        printf("Error %d / %d\n", status->error.domain, status->error.code);
    }
}

static void start_replication() {
    CBLDatabase* database = kDatabase;

    /*
    * This requires Sync Gateway running with the following config, or equivalent:
    *
    * {
    *     "log":["*"],
    *     "databases": {
    *         "db": {
    *             "server":"walrus:",
    *             "users": {
    *                 "GUEST": {"disabled": false, "admin_channels": ["*"] }
    *             }
    *         }
    *     }
    * }
    */

    // tag::replication[]
    // NOTE: No error handling, for brevity (see getting started)
    // Note: Android emulator needs to use 10.0.2.2 for 127.0.0.1 (10.0.3.2 for GenyMotion)
    CBLError err;
    CBLEndpoint* targetEndpoint = CBLEndpoint_CreateWithURL(FLSTR("wss://1r7gqolb2uq6dl3n.apps.cloud.couchbase.com:4984/projects"), &err);
    if (!targetEndpoint) {
        // Failed to create replicator
        fprintf(stderr, "Error creating replicator (%d / %d)\n", err.domain, err.code);
        FLSliceResult msg = CBLError_Message(&err);
        fprintf(stderr, "%.*s\n", (int)msg.size, (const char*)msg.buf);
        FLSliceResult_Release(msg);
        return 1;
    }
    //configure basic auth
    CBLReplicatorConfiguration replConfig;
    CBLAuthenticator* basicAuth = CBLAuth_CreatePassword(FLSTR("demo@example.com"), FLSTR("P@ssw0rd12"));
    memset(&replConfig, 0, sizeof(replConfig));
    //replConfig.database = database;
    replConfig.endpoint = targetEndpoint;
    replConfig.authenticator = basicAuth;

    CBLReplicationCollection collectionConfig;
    memset(&collectionConfig, 0, sizeof(CBLReplicationCollection));
    CBLCollection* collection = CBLDatabase_DefaultCollection(database, &err);
    collectionConfig.collection = collection;
    replConfig.collectionCount = 1;
    replConfig.collections = &collectionConfig;

    replConfig.replicatorType = kCBLReplicatorTypePushAndPull;
    replConfig.continuous = true;
    CBLReplicator* replicator = CBLReplicator_Create(&replConfig, &err);
    CBLAuth_Free(basicAuth);
    CBLEndpoint_Free(targetEndpoint);
    if (!replicator) {
        // Failed to create replicator, do error handling as above
        fprintf(stderr, "Error creating replicator (%d / %d)\n", err.domain, err.code);
        FLSliceResult msg = CBLError_Message(&err);
        fprintf(stderr, "%.*s\n", (int)msg.size, (const char*)msg.buf);
        FLSliceResult_Release(msg);
        return 1;
    }
    CBLListenerToken* token = CBLReplicator_AddChangeListener(replicator, getting_started_change_listener, NULL);
    CBLReplicator_Start(replicator, false);
    // end::replication[]

   // stop_replicator(replicator);
}

// Console logging domain methods are not applicable to C





int main(int argc, char** argv) {
    create_new_database();
    create_document();
    start_replication();
    //replicator_property_encryption();
    while (true) {
        Sleep(10000);
        create_document();
    }

    return 0;
}