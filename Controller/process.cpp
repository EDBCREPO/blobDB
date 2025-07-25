#pragma once

/*────────────────────────────────────────────────────────────────────────────*/

namespace blobDB { void reading_folder_list( string_t dir ) {

    auto idx = type::bind( new uint(0) );

fs::read_folder( dir, [=]( string_t name ){ try {

    if( !regex::test( name, "^blob_" ) ){ throw ""; }
    if( ws_list.empty()                ){ throw ""; }

    *idx = ( *idx + 1 ) % ws_list.size();
    auto raw = path::join( dir, name );

    if( is_file_expired(raw) )
      { fs::remove_file(raw); throw ""; }

    apify::add( ws_list[*idx]->data )
    .emit( "PROCESS", "/api/v1/file", raw );

} catch(...) {} }); }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace blobDB { void run_v1_process() { 
process::add( coroutine::add( COROUTINE(){
coBegin 

    coDelay( TIME_HOURS(string::to_ulong(process::env::get("TMP_TIMEOUT"))) );
    reading_folder_list( process::env::get("STORAGE_PATH") );

coGoto(0) ; coFinish
})); }}

/*────────────────────────────────────────────────────────────────────────────*/
