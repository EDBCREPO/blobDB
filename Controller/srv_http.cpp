#pragma once

/*────────────────────────────────────────────────────────────────────────────*/

namespace blobDB { express_tcp_t run_v1_http_server_api_routine(){
    auto app = express::http::add();

    /*.........................................................................*/

    auto resolve = ([=]( express_http_t cli, object_t hdr ){ try {
    if( hdr.empty() ){ throw ""; }

        cli.write_header( 200, header_t({ { "Content-Type", path::mimetype(".json") } }) );
        cli.write( json::stringify( object_t({
            { "message", HTTP_NODEPP::_get_http_status(200) },
            { "status" , 200 }, { "data", hdr } 
        }) ));

    } catch(...) {

        cli.write_header( 404, header_t({ { "Content-Type", path::mimetype(".json") } }) );
        cli.write( json::stringify( object_t({
            { "status" , 404 }, { "data", "something went wrong" },
            { "message", HTTP_NODEPP::_get_http_status(404) }
        }) ));

    }});

    /*.........................................................................*/

    app.POST( "/v1/append/raw", [=]( express_http_t cli ){ try {

        if( !cli.headers.has("Content-Length") )
          { throw except_t( "Invalid Content-Length" ); }

        auto len = string::to_ulong( cli.headers["Content-Length"] );
        auto tmp = cli.read( min( (ulong) CHUNK_SIZE, len ) );
        auto data= json::parse( cli.query ); cli.done();

        resolve( cli, parse_query_raw( tmp, data ) );

    } catch( except_t err ) { cli.params["ERRNO"]=err.data(); } });

    /*.........................................................................*/

    app.POST( "/v1/append/path", [=]( express_http_t cli ){ try {
        if( !cli.headers.has("Content-Length") )
          { throw except_t( "Invalid Content-Length" ); }

        auto len = string::to_ulong( cli.headers["Content-Length"] );

        if( regex::test( cli.headers["Content-Type"], "boundary" ) )
          { throw except_t( "multi-part invalid" ); }

        auto data=json::parse( url::normalize( cli.read(len) ) );

        if( !data.has("path") ){ throw except_t( "Invalid PATH" ); }

        if( url::is_valid(data["path"].as<string_t>()) ){
        if( url::protocol(data["path"].as<string_t>())=="http" ){

            fetch_t args; cli.done();
                    args.method = "GET";
                    args.headers= cli.headers;
                    args.url    = data["path"].as<string_t>();

            http::fetch(args).then([=]( http_t raw ){ try {

                if( raw.headers.has("Content-Disposition") ){
                    auto mem = regex::get_memory(raw.headers["Content-Disposition"], "filename=\"([^\"]+)\"");
                if( !mem.empty() && !mem[0]        .empty() ){ data["filename"] = mem[0]; }}
                if( data["filename"].as<string_t>().empty() ){ data["filename"] = encoder::key::generate(32); }

                data["mimetype"]= raw.headers["Content-Type"];
                auto obj        = parse_query_file( data, raw );
                resolve( cli, obj ); return;

            } catch(...) {  } resolve( cli, nullptr );
            }).fail([=](...){ resolve( cli, nullptr ); });

        } elif( url::protocol(data["path"].as<string_t>())=="https" ){

            fetch_t args; ssl_t ssl; cli.done();
                    args.method = "GET";
                    args.headers= cli.headers;
                    args.url    = data["path"].as<string_t>();

            https::fetch(args,&ssl).then([=]( https_t raw ){ try {

                if( raw.headers.has("Content-Disposition") ){
                    auto mem = regex::get_memory(raw.headers["Content-Disposition"],"filename=\"([^\"]+)\"");
                if( !mem.empty() && !mem[0]        .empty() ){ data["filename"] = mem[0]; }}
                if( data["filename"].as<string_t>().empty() ){ data["filename"] = encoder::key::generate(32); }

                data["mimetype"]= raw.headers["Content-Type"];
                auto obj        = parse_query_file( data, raw );
                resolve( cli, obj ); return;

            } catch(...) {  } resolve( cli, nullptr );
            }).fail([=](...){ resolve( cli, nullptr ); });

        } throw except_t(); } elif( fs::exists_file(data["path"].as<string_t>()) ){

            if( fs::is_folder(data["path"].as<string_t>()) )
              { throw except_t("Path refers to a folder"); }

            data["filename"]= path::basename  ( data["path"].as<string_t>(), "[.].+$" );
            data["mimetype"]= path::mimetype  ( data["path"].as<string_t>() );
            auto file       = fs::readable    ( data["path"].as<string_t>() );
            auto obj        = parse_query_file( data, file );

            resolve( cli, obj ); cli.done();

        } throw except_t();

    } catch( except_t err ) { cli.params["ERRNO"]=err.data(); } });

    /*.........................................................................*/

    app.POST( "/v1/append/stream", [=]( express_http_t cli ){ try {
        if( !cli.headers.has("Content-Length") )
          { throw except_t("invalid file length"); }

        auto len = string::to_ulong( cli.headers["Content-Length"] );
        auto SIZE= string::to_ulong( process::env::get("MAX_SIZE") );

        if( !regex::test( cli.headers["Content-Type"], "boundary" ) )
          { throw except_t("invalid boundary"); }
        if( len > CHUNK_MB(SIZE) )
          { throw except_t("file is too large"); }

        cli.parse_stream().then([=]( object_t data ){ try {
            auto obj = parse_query_stream( data );
            resolve( cli, obj ); return;
        } catch(...) {  } resolve( cli, nullptr );
        }).fail([=](...){ resolve( cli, nullptr ); }); cli.done();

    } catch( except_t err ) { cli.params["ERRNO"]=err.data(); } });

    /*.........................................................................*/

    app.GET( "/v1/file/:FID", [=]( express_http_t cli ){ try {
        auto dir = path::join(process::env::get("STORAGE_PATH"),"blob_"+cli.params["FID"]);

        if( !fs::exists_file(dir) || is_file_expired(dir) )
          { throw except_t("file not found"); }

    string_t pass = cli.query.has("pass") ? cli.query["pass"] : nullptr;
        auto file = fs::readable( dir ); auto raw1 = file.read_line();
        auto hdr  = json::parse( encoder::base64::btoa( raw1 ) );

        auto raw2 = file.read_line();
        auto hash = crypto::hash::SHA256(); 
        auto sha  = raw2; auto verified = true; 
        auto start=( raw1.size() + raw2.size() );
        auto len  =  file.size()-( raw1.size() + raw2.size() );
        hash.update( json::stringify(hdr) ); hash.update(pass);

        if( is_expired( hdr ) )
          { even_emit_erase(dir); throw except_t("file not found"); }

        if( hdr["ENC"].as<bool>() && memcmp( sha.get(), hash.get().get(), hash.get().size() )!=0 )
          { verified = false; }

        if( is_expired( hdr ) )
          { even_emit_erase(dir); throw except_t("file not found"); }

        cli.status(200).sendJSON( object_t({
            { "message"   , HTTP_NODEPP::_get_http_status(200) },
            { "data"      , object_t({
            { "length"    , string::to_string(len) },
            { "expiration", hdr["EXP"] },
            { "creation"  , hdr["NOW"] },
            { "encrypted" , hdr["ENC"] },
            { "mimetype"  , hdr["TYP"] },
            { "filename"  , hdr["NME"] },
            { "fid"       , hdr["FID"] },
            { "verified"  , verified   }
        }) }, { "status" , 200 } }) );

    } catch( except_t err ) { cli.params["ERRNO"]=err.data(); } });

    /*.........................................................................*/

    app.REMOVE( "/v1/file/:FID", [=]( express_http_t cli ){ try {
        auto dir = path::join(process::env::get("STORAGE_PATH"),"blob_"+cli.params["FID"]);

        if( !fs::exists_file(dir) ){ throw except_t("file not found"); }
             fs::remove_file(dir);

        cli.status(200).sendJSON( object_t({
            { "message", HTTP_NODEPP::_get_http_status(200) },
            { "data"   , "removed" }, { "status" , 200 } 
        }) );

    } catch( except_t err ) { cli.params["ERRNO"]=err.data(); } });

    /*.........................................................................*/

    app.ALL([=]( express_http_t cli ){ try {

        if( cli.params["ERRNO"].empty() )
          { cli.params["ERRNO"]="something went wrong"; }

        cli.status(404).sendJSON( object_t({
            { "data"   , cli.params["ERRNO"] }, { "status" , 404 },
            { "message", HTTP_NODEPP::_get_http_status(404) }
        }) );

    } catch(...) {

        cli.status(404).send("something went wrong");

    } });

    /*.........................................................................*/

    return app;
}}

/*────────────────────────────────────────────────────────────────────────────*/

namespace blobDB { express_tcp_t run_v1_http_server_routine(){

    auto app = express::http::add();

    /*.........................................................................*/

    app.GET( "/file/:FID", [=]( express_http_t cli ){ try {

        auto dir = path::join(process::env::get("STORAGE_PATH"),"blob_"+cli.params["FID"]);

        if( !fs::exists_file(dir) || is_file_expired(dir) )
          { throw except_t("file not found"); }

    string_t pass = cli.query.has("pass") ? cli.query["pass"] : nullptr;
        auto file = fs::readable( dir ); auto raw1 = file.read_line();
        auto hdr  = json::parse( encoder::base64::btoa( raw1 ) );

        auto raw2 = file.read_line();
        auto hash = crypto::hash::SHA256(); auto sha = raw2;
        auto start=( raw1.size() + raw2.size() );
        auto len  =  file.size()-( raw1.size() + raw2.size() );
        hash.update( json::stringify(hdr) ); hash.update(pass);

        if( is_expired( hdr ) )
          { even_emit_erase(dir); throw except_t("file not found"); }

        if( hdr["ENC"].as<bool>() && memcmp( sha.get(), hash.get().get(), hash.get().size() )!=0 )
          { throw except_t("encrypted file"); }

        if( cli.query.has("save") )
          { cli.header( "Content-Disposition", regex::format( "attachment; filename=\"${0}\"", hdr["NME"].as<string_t>() )); }

        if( cli.headers.has("Range") ){

            array_t<string_t> range = regex::match_all(cli.headers["Range"],"\\d+",true);
               ulong rang[3]; rang[0] = string::to_ulong( range[0] );
                     rang[1] =min(rang[0]+CHUNK_MB(10),len-1);
                     rang[2] =min(rang[0]+CHUNK_MB(10),len  );

            cli.header( "Content-Type" , hdr["TYP"].as<string_t>() ); cli.header( "Accept-Range","bytes" )
               .header( "Content-Range", string::format("bytes %lu-%lu/%lu",rang[0],rang[1],len) )
               .header( "Cache-Control", "public, max-age=604800" );

            file.set_range( start + rang[0], len - rang[2] );

            if( pass.empty() ){

                cli.status(206).send(); stream::pipe( file, cli );

            } else {
                auto idx = type::bind( new ulong(0) );
                auto key = hash.get();

                file.onData([=]( string_t data ){ for( auto &x: data ){
                    x^=key[*idx]; (*idx)=(*idx+1)%key.size();
                }   cli.write( data ); });

                cli.status(206).send(); stream::pipe( file,cli );

            }

        } else {

            cli.header( "Content-Type"  , hdr["TYP"].as<string_t>() )
               .header( "Cache-Control" , "public, max-age=604800" )
               .header( "Content-Length", string::to_string(len) );

            if( pass.empty() ){ cli.sendStream(file); } else {

                auto xenc = crypto::encrypt::XOR( hash.get() );

                file.onData([=]( string_t data ){ xenc.update(data); });
                xenc.onData([=]( string_t data ){ cli.write  (data); });
                file.onDrain.once([=](){ xenc.free(); });

                cli.send(); stream::pipe( file );

            }

        }

    } catch( except_t err ) {

        cli.status(404).sendJSON( object_t({
            { "data"   , err.data() }, { "status" , 404 },
            { "message", HTTP_NODEPP::_get_http_status(404) }
        }) );

    } });

    /*.........................................................................*/

    app.USE( express::http::file("./View") );
    return app;

}}

/*────────────────────────────────────────────────────────────────────────────*/

namespace blobDB { void run_v1_http_server(){

    auto penv= process::env::get( "HTTP_PORT" );
    auto port= string::to_uint( penv );
    auto app = express::http::add();

    /*.........................................................................*/

    app.USE( "/api", run_v1_http_server_api_routine() );
    app.USE(         run_v1_http_server_routine() );

    /*.........................................................................*/

    app.listen( "localhost", port, [=](...){
        console::log( regex::format(
            "<> http://localhost:${0}", port
        ));
    });

}}

/*────────────────────────────────────────────────────────────────────────────*/
