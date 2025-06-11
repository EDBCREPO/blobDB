#pragma once

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { void even_emit_erase( string_t path ) { try {
    if( ws_client == nullptr ){ throw ""; }
    apify::add( *ws_client ).emit( "DELETE", "/api/v1/file", path );
} catch(...) {} }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { bool is_expired( object_t hdr ){ try {

    auto EXP = string::to_ulong( hdr["EXP"].as<string_t>() );
    auto WON = string::to_ulong( hdr["NOW"].as<string_t>() );
    auto NOW = date  ::now();

    if( EXP == 0 || NOW <= ( WON + EXP ) ) { return false; }

} catch(...) {} return true; }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { bool is_file_expired( string_t dir ) { try {

    if( string::to_int( process::env::get("EXP_TIME") )<=0 ){ return false; }

    auto EXP = string::to_ulong( process::env::get("EXP_TIME") );
    auto WON = fs  ::file_creation_time(dir);
    auto NOW = date::now();

    if( EXP==0 || NOW<=( WON + EXP ) ){ return false; }

} catch(...) {} return true; }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { array_t<object_t> parse_query_stream( object_t data ){ try {
    array_t<object_t> out;

    /*.........................................................................*/

    for( auto x: data.keys() ){ if( x=="file" ){ continue; }
    if ( data[x].is<array_t<object_t>>()      ){
    for( auto y: data[x]  .as<array_t<object_t>>() ){
         fs::remove_file( y["path"].as<string_t>() );
    } data.erase(x); }}

    /*.........................................................................*/

    if( !data.has("filename") ){ data["filename"]=encoder::key::generate(32); }
    if( !data.has("password") ){ data["password"]=string_t(nullptr); }
    if( !data.has("mimetype") ){ data["mimetype"]="text/plain"; }
    if( !data.has("expire")   ){ data["expire"]  ="0"; }

    /*.........................................................................*/

    for( auto x: data["file"].as<array_t<object_t>>() ){ object_t obj;

        auto name  = "blob_"+path::basename(x["path"].as<string_t>(),".tmp");
        auto time  = string::to_uint(data["expire"].as<string_t>());

        obj["NME"] = x["filename"].as<string_t>(); obj["FID"]=name.slice(5);
        obj["DIR"] = path::join( process::env::get("STORAGE_PATH"), name );
        obj["TYP"] = x["mimetype"].as<string_t>(); out.push( obj );
        obj["EXP"] = string::to_string( date::now() + time ); 
        obj["NOW"] = string::to_string( date::now() );

        if( data["password"].as<string_t>().empty() ){ obj["ENC"] = false;

            auto fout = fs::writable( obj["DIR"].as<string_t>() );
            auto finp = fs::readable( x["path"] .as<string_t>() );
            auto hash = crypto::hash::SHA256();

            hash.update( json::stringify(obj) );
            hash.update( data["password"].as<string_t>() );

            finp.onDrain.once([=](){ fs::remove_file( x["path"].as<string_t>() ); });
            fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
            fout.write( hash.get() + "\n" );

            stream::pipe( finp, fout );

        } else { obj["ENC"] = true;

            auto fout = fs::writable( obj["DIR"].as<string_t>() );
            auto finp = fs::readable( x["path"] .as<string_t>() );
            auto hash = crypto::hash::SHA256();

            hash.update( json::stringify(obj) );
            hash.update( data["password"].as<string_t>() );
            auto xenc = crypto::encrypt::XOR( hash.get() );

            finp.onDrain.once([=](){ fs::remove_file( x["path"].as<string_t>() ); });
            fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
            fout.write( hash.get() + "\n" );

            finp.onData([=]( string_t data ){ xenc.update(data); });
            xenc.onData([=]( string_t data ){ fout.write (data); });
            finp.onDrain.once([=](){ xenc.free(); });

            stream::pipe( finp );

        }

    }

    /*.........................................................................*/

    return out;

} catch( except_t err ) {

    for( auto x: data.keys() ){
    if ( data[x].is<array_t<object_t>>() ){
    for( auto y: data[x].as<array_t<object_t>>() ){
         fs::remove_file( y["path"].as<string_t>() );
    }}}

} throw except_t(); return nullptr; }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB { template< class T >
object_t parse_query_file( object_t data, T& finp ){ try { object_t obj;

    /*.........................................................................*/

    if( !data.has("filename") ){ data["filename"]=encoder::key::generate(32); }
    if( !data.has("password") ){ data["password"]=string_t(nullptr); }
    if( !data.has("mimetype") ){ data["mimetype"]="text/plain"; }
    if( !data.has("expire")   ){ data["expire"]  ="0"; }

    /*.........................................................................*/

    auto hash = crypto::hash::SHA256();
         hash.update( json::stringify(data) );
         hash.update( encoder::key::generate(32) );
         hash.update( string::to_string( process::now() ) );
    
    auto name = "blob_" + hash.get();
    auto time = string::to_uint(data["expire"].as<string_t>());

    /*.........................................................................*/

    obj["DIR"] = path::join( process::env::get("STORAGE_PATH"), name );
    obj["EXP"] = string::to_string(time); obj["FID"] = name.slice(5);
    obj["NOW"] = string::to_string( date::now() );
    obj["NME"] = data["filename"].as<string_t>();
    obj["TYP"] = data["mimetype"].as<string_t>();

    /*.........................................................................*/

    if( data["password"].as<string_t>().empty() ){ obj["ENC"] = false;

        auto fout = fs::writable( obj["DIR"].as<string_t>() );
        auto hash = crypto::hash::SHA256();

        hash.update( json::stringify(obj) );
        hash.update( data["password"].as<string_t>() );

        fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
        fout.write( hash.get() + "\n" );

        stream::pipe( finp, fout );

    } else { obj["ENC"] = true;

        auto fout = fs::writable( obj["DIR"].as<string_t>() );
        auto pass = data["password"].as<string_t>();
        auto hash = crypto::hash::SHA256();

        hash.update( json::stringify(obj) );
        hash.update( data["password"].as<string_t>() );
        auto xenc = crypto::encrypt::XOR( hash.get() );

        fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
        fout.write( hash.get() + "\n" );

        finp.onData([=]( string_t data ){ xenc.update(data); });
        xenc.onData([=]( string_t data ){ fout.write (data); });
        finp.onDrain.once([=](){ xenc.free(); });

        stream::pipe( finp );

    }

    /*.........................................................................*/

    return obj;

} catch(...) {} throw except_t(); }}

/*────────────────────────────────────────────────────────────────────────────*/

namespace fileDB {
object_t parse_query_raw( string_t& finp, object_t data ){ try { object_t obj;

    /*.........................................................................*/

    if( !data.has("filename") ){ data["filename"]=encoder::key::generate(32); }
    if( !data.has("password") ){ data["password"]=string_t(nullptr); }
    if( !data.has("mimetype") ){ data["mimetype"]="text/plain"; }
    if( !data.has("expire")   ){ data["expire"]  ="0"; }

    /*.........................................................................*/
    
    auto hash = crypto::hash::SHA256();
         hash.update( json::stringify(data) );
         hash.update( encoder::key::generate(32) );
         hash.update( string::to_string( process::now() ) );
    
    auto name = "blob_" + hash.get();
    auto time = string::to_uint(data["expire"].as<string_t>());

    /*.........................................................................*/

    obj["DIR"] = path::join( process::env::get("STORAGE_PATH"), name );
    obj["EXP"] = string::to_string(time); obj["FID"] = name.slice(5);
    obj["NOW"] = string::to_string( date::now() );
    obj["NME"] = data["filename"].as<string_t>();
    obj["TYP"] = data["mimetype"].as<string_t>();

    /*.........................................................................*/

    if( data["password"].as<string_t>().empty() ){ obj["ENC"] = false;

        auto fout = fs::writable( obj["DIR"].as<string_t>() );
        auto hash = crypto::hash::SHA256();

        hash.update( json::stringify(obj) );
        hash.update( data["password"].as<string_t>() );

        fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
        fout.write( hash.get() + "\n" ); fout.write( finp );

    } else { obj["ENC"] = true;

        auto fout = fs::writable( obj["DIR"].as<string_t>() );
        auto hash = crypto::hash::SHA256();

        hash.update( json::stringify(obj) );
        hash.update( data["password"].as<string_t>() );

        fout.write( encoder::base64::atob(json::stringify(obj)) + "\n" );
        fout.write( hash.get() + "\n" ); 
        fout.write( encoder::XOR::atob( finp, hash.get() ) );

    }

    /*.........................................................................*/

    return obj;

} catch( except_t err ) {} throw except_t(); }}

/*────────────────────────────────────────────────────────────────────────────*/
