#define MAX_FILENO 10485760

/*────────────────────────────────────────────────────────────────────────────*/

#include <nodepp/nodepp.h>
#include <nodepp/cluster.h>
#include <express/http.h>
#include <nodepp/https.h>
#include <nodepp/timer.h>
#include <nodepp/http.h>
#include <nodepp/date.h>
#include <apify/apify.h>
#include <nodepp/ws.h>

using namespace nodepp;

queue_t<ws_t> ws_list ; ptr_t<ws_t> ws_client;
#include "../Controller/import.cpp"

/*────────────────────────────────────────────────────────────────────────────*/

void onMain() {
    process::env::init(".env");
    fileDB::run_v1_cluster();
}

/*────────────────────────────────────────────────────────────────────────────*/
