BASE=/local/data/ichilevi/git_repos/mysql57/
BUILD=$BASE/build2/
SRC=$BASE/mysql-server-1/

cd $BUILD/sql

clang++  -DHAVE_CONFIG_H -DHAVE_LIBEVENT1 -DHAVE_REPLICATION -DMYSQL_SERVER -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I/local/data/ichilevi/local/llvm/include -I$BUILD/include -I$SRC/extra/rapidjson/include -I$BUILD/libbinlogevents/include -I$SRC/libbinlogevents/export -I$SRC/include -I$SRC/sql/conn_handler -I$SRC/libbinlogevents/include -I$SRC/sql -I$SRC/sql/auth -I$SRC/regex -I$SRC/zlib -I$SRC/extra/yassl/include -I$SRC/extra/yassl/taocrypt/include -I$BUILD/sql -I$SRC/extra/lz4  -fPIC -Wall -Wextra -Wformat-security -Wvla -Woverloaded-virtual -Wno-unused-parameter -Wno-null-conversion -Wno-unused-private-field -g -fno-omit-frame-pointer -fno-strict-aliasing -DENABLED_DEBUG_SYNC -DSAFE_MUTEX   -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -DHAVE_YASSL -DYASSL_PREFIX -DHAVE_OPENSSL -DMULTI_THREADED -S -emit-llvm -o $BASE/sql_executor.cc.ll -c $SRC/sql/sql_executor.cc
