#pragma once

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { apify_host_t<ws_t> run_v1_ws_server_routine(){
    apify_host_t<ws_t> app;

    app.on( "DELETE", "/api/v1/file", [=]( apify_t<ws_t> cli ){ try {
        fs::remove_file( cli.message );
    } catch(...) {} });

    return app;
}}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { void run_v1_ws_server(){

    auto penv= process::env::get( "APIF_PORT" );
    auto port= string::to_uint( penv );
    auto app = apify::add<ws_t>();
    auto ws  = ws::server();

    /*.........................................................................*/

    app.add( run_v1_ws_server_routine() );

    /*.........................................................................*/

    ws.onConnect([=]( ws_t cli ){
        ws_list.push( cli ); auto ID = ws_list.last();
        cli.onDrain([=](...){ ws_list.erase( ID ); });
        cli.onData ([=]( string_t data ){ app.next( cli, data ); });
    });

    /*.........................................................................*/

    ws.listen( "localhost", port, [=](...){
        console::log( regex::format(
            "<> ws://localhost:${0}", port
        ));
    });

}}

/*────────────────────────────────────────────────────────────────────────────*/
