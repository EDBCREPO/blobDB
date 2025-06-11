#pragma once

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { apify_host_t<ws_t> run_v1_ws_client_routine(){
    apify_host_t<ws_t> app;

    app.on( "PROCESS", "/api/v1/file", [=]( apify_t<ws_t> cli ){ try {
        auto file = fs::readable( cli.message );
        auto hdr  = json::parse( encoder::base64::btoa(file.read_line()) );

        if( is_expired(hdr) ){ fs::remove_file( cli.message ); }

    } catch(...) {} });

    return app;
}}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { void run_v1_ws_client(){ try {

    auto penv= process::env::get( "APIF_PORT" );
    auto port= string::to_uint( penv );
    auto app = apify::add<ws_t>();

    /*.........................................................................*/

    auto cli = ws::client( regex::format( "ws://localhost:${0}", port ));

    /*.........................................................................*/

    app.add( run_v1_ws_client_routine() );

    /*.........................................................................*/

    cli.onConnect([=]( ws_t cli ){ ws_client = type::bind( cli );
    cli.onData   ([=]( string_t data ){ app.next( cli, data ); });
    cli.onError  ([=](...){ timer::timeout([=](){ run_v1_ws_client(); },1000); });
    cli.onDrain  ([=](...){ timer::timeout([=](){ run_v1_ws_client(); },1000); }); });

} catch(...) {
    timer::timeout([=](){ run_v1_ws_client(); },1000);
}}}

/*────────────────────────────────────────────────────────────────────────────*/
