/*
The MIT License (MIT)

Copyright 2023 Frédéric Morvan

Permission is hereby granted, free of charge, to any
person obtaining a copy of this software and associated
documentation files (the “Software”), to deal in the
Software without restriction, including without limitation
the rights to use,copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice
shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>

#define DEFAULT_TEXT_EXTENSIONS  "ini,txt,json,xml"
#define PATH_SEPARATOR  '/'

enum res_type
{
    RES_TYPE_BINARY = 0,
    RES_TYPE_TEXT
};




struct context
{
    const char* text_extension;
    const char* root;
    const char* prefix;
    char*       define;
    char        define_free;


    FILE*       header;
    const char* header_path;
    const char* header_filename;
    struct stat header_stat;

    FILE*       source;
    const char* source_path;
    const char* source_filename;
    struct stat source_stat;
};






static const char* filename( const char* path )
{
    const char* name;

    name = strrchr( path, PATH_SEPARATOR );
    if( name == NULL )
        name = path;
    else
        ++name;

    return name;
}

static const char* file_extension( const char* filename )
{
    const char* ext;

    ext = strrchr( filename, '.' );
    if( ext  )
        ++ext;
    return ext;
}

static size_t file_size( FILE* f )
{
    size_t sz;

    fseek( f, 0, SEEK_END);
    sz = ftell(f);

    fseek( f, 0, SEEK_SET );

    return sz;

}


static char* _asprintf( const char* format, ... )
{
    int r;
    va_list list;
    char* str;

    str = NULL;

    va_start( list, format );
    r = vsnprintf( NULL, 0, format, list );
    if( r < 0 )
    {
        perror( "vsnprintf" );
        goto LBL_FAILED;
    }

    ++r;
    str = malloc( r );
    if( str == NULL )
    {
        perror( "malloc" );
        goto LBL_FAILED_MALLOC;
    }

    va_start( list, format );

    r = vsnprintf(str, r, format, list);
    if( r < 0 )
    {
        perror( "vsnprintf" );
        goto LBL_FAILED_PRINTF_MALLOC;
    }

    va_end( list );

    return str;

LBL_FAILED_PRINTF_MALLOC:
    free( str );
    str = NULL;

LBL_FAILED_MALLOC:

LBL_FAILED:
    va_end( list );
    return NULL;
}

static void str_santize( char* str )
{
    int len;
    int i;

    len = strlen( str );
    for( i = 0; i < len; ++i )
    {
        if( isalnum(str[i]) == 0 )
            str[i] = '_';
    }
}

static void str_upper( char* str )
{
    int len;
    int i;

    len = strlen( str );
    for( i = 0; i < len; ++i )
    {
        if( isalpha(str[i]) )
            str[i] = toupper( str[i] );
    }
}

static void print_help()
{
    fprintf( stdout, "dir2c [-h] [-t EXT1,EXT2,...] [-p PREFIX] [-d DEFINE] -H HEADER_FILENAME -s SOURCE_FILENAME DIR_PATH\n" );
    fprintf( stdout, "\n" );
    fprintf( stdout, "-h                  : print this message\n");
    fprintf( stdout, "-t EXT1,EXT2,...    : if filenames have this extension, add a terminal NULL byte to the C constant. defaults is '%s'\n", DEFAULT_TEXT_EXTENSIONS);
    fprintf( stdout, "-p PREFIX           : prefix const name with PREFIX\n");
    fprintf( stdout, "-d DEFINE           : set header define guard to DEFINE. default is uppercase header filename without extension\n");
    fprintf( stdout, "-H FILENAME         : write header to FILENAME\n");
    fprintf( stdout, "-s FILENAME         : write source to FILENAME\n");
    fprintf( stdout, "DIRPATH             : the directory to process, if not specified then process current directory\n");
    fflush( stdout );
}


static void print_declaration( FILE* f, enum res_type type, const char* prefix, const char* name, size_t size )
{
    fprintf( f, "const char " );
    if( prefix)
        fprintf( f, "%s_", prefix );

    fprintf( f, "%s[%" PRIu64, name, size );

    if( type == RES_TYPE_TEXT )
        fprintf( f, "+1");

    fprintf( f, "]" );
}

int file_is_text( struct context* ctx,  const char* filename )
{
    const char* ext;
    int ext_len;
    const char* p;

    ext = file_extension( filename );
    if( ext == NULL )
        return 0;

    p = strstr( ctx->text_extension, ext );
    if( p == NULL )
        return 0;

    ext_len = strlen(ext);
    if( p == ctx->text_extension )
    {
        if( (p[ext_len] == ',') || (p[ext_len] == 0) )
            return 1;
    }
    else if( p )
    {
        if( (p[-1] == ',') && ((p[ext_len] == ',') || (p[ext_len] == 0)) )
            return 1;
    }
    return 0;
}

static int process_file( struct context* ctx, const char* dirpath, char* name, const char* prefix )
{
    int r;
    char* path;
    FILE* f;
    size_t f_size;
    enum res_type f_type;
    struct stat stat;


    r = 0;
    path = NULL;
    f = NULL;
    f_size = 0;
    f_type = RES_TYPE_BINARY;


    // path
    path = _asprintf( "%s%c%s", dirpath, PATH_SEPARATOR, name );
    if( path == NULL  )
        goto LBL_FAILED_PATH;



    // open
    f = fopen( path, "r" );
    if( f == NULL )
    {
        perror( "fopen" );
        goto LBL_FAILED_FOPEN;
    }

    // is header file ?
    if( strcmp( name, ctx->header_filename) == 0 )
    {
        r = fstat( fileno(f), &stat );
        if( r == -1 )
            goto LBL_FAILED_FSTAT;

        if( (stat.st_dev == ctx->header_stat.st_dev) && (stat.st_ino == ctx->header_stat.st_ino) )
            goto LBL_SKIP;
    }
    // is source file ?
    else if( strcmp( name, ctx->source_filename) == 0 )
    {
        r = fstat( fileno(f), &stat );
        if( r == -1 )
            goto LBL_FAILED_FSTAT;

        if( (stat.st_dev == ctx->source_stat.st_dev) && (stat.st_ino == ctx->source_stat.st_ino) )
            goto LBL_SKIP;
    }


    // size
    f_size = file_size( f );
    if( f_size == 0 )
    {
        fprintf( stdout, "Ignoring empty file '%s'\n", path );
        goto LBL_SKIP;
    }


    // type
    if( file_is_text( ctx, name ) )
        f_type = RES_TYPE_TEXT;



    // sanitize
    str_santize( name );

    // header
    fprintf( ctx->header, "extern ");
    print_declaration( ctx->header, f_type, prefix, name, f_size );
    fprintf( ctx->header, ";\n");


    // source
    print_declaration( ctx->source, f_type, prefix, name, f_size );
    fprintf( ctx->source, " = {" );
    for(;;)
    {
        char c;
        r = fread( &c, 1, 1, f);
        if( r != 1 )
        {
            if( feof(f) )
                break;
            perror( "fread" );
            goto LBL_FAILED_FREAD;
        }


        fprintf( ctx->source, "0x%02x,", c & 0xff );
    }
    switch( f_type )
    {
        case RES_TYPE_BINARY:   fseek( ctx->source, -1, SEEK_CUR ); break;
        case RES_TYPE_TEXT:     fprintf( ctx->source, "0x00" ); break;
    }
    fprintf( ctx->source, "};\n" );

LBL_SKIP:
    // close
    fclose( f );
    f = NULL;
    free( path );
    path = NULL;
    f_size = 0;
    f_type = RES_TYPE_BINARY;
    return 0;

LBL_FAILED_FREAD:

LBL_FAILED_FSTAT:
    fclose( f );
    f = NULL;

LBL_FAILED_FOPEN:
    free( path );
    path = NULL;

LBL_FAILED_PATH:
    return -1;
}


static int process_dir( struct context* ctx, const char* dirpath, const char* prefix )
{
    int r;
    DIR* dir;
    struct dirent* e;
    char* sub_dirpath;
    char* sub_prefix;


    r = 0;
    dir = NULL;
    e = NULL;
    sub_dirpath = NULL;
    sub_prefix = NULL;


#ifdef __DEBUG__
    fprintf( stdout, "Entering '%s'\n", dirpath );
    fflush( stdout );
#endif


    dir = opendir( dirpath );
    if( dir == NULL )
    {
        perror( "opendir" );
        goto LBL_FAILED_OPENDIR;
    }

    // files
    for(;;)
    {
        e = readdir( dir );
        if( e == NULL )
            break;

        if( e->d_type != DT_REG )
            continue;




        r = process_file( ctx, dirpath, e->d_name, prefix );
        if( r != 0 )
            goto LBL_FAILED_PROCESS_FILE;
    }


    // subdirs
    rewinddir( dir );
    for(;;)
    {
        e = readdir( dir );
        if( e == NULL )
            break;

        if( e->d_type != DT_DIR )
            continue;

        if( (strcmp(e->d_name, ".") ==0) || (strcmp(e->d_name, "..") == 0) )
            continue;


        // subpath
        sub_dirpath = _asprintf( "%s%c%s", dirpath, PATH_SEPARATOR, e->d_name );
        if( sub_dirpath == NULL )
            goto LBL_FAILED_SUBDIR_PATH;

        // subprefix
        str_santize( e->d_name );
        if( prefix )
            sub_prefix = _asprintf( "%s_%s", prefix, e->d_name );
        else
            sub_prefix = _asprintf( "%s", e->d_name );
        if( sub_prefix == NULL )
            goto LBL_FAILED_SUBDIR_PREFIX;

        r = process_dir( ctx, sub_dirpath, sub_prefix );
        if( r != 0 )
            goto LBL_FAILED_SUB_WALK;

        free( sub_dirpath );
        sub_dirpath = NULL;

        free( sub_prefix );
        sub_prefix = NULL;
    }

    if( sub_prefix )
    {
        free( sub_prefix );
        sub_prefix = NULL;
    }



    closedir( dir );
    dir = NULL;
    return 0;

LBL_FAILED_SUB_WALK:
    free( sub_prefix );
    sub_prefix = NULL;

LBL_FAILED_SUBDIR_PREFIX:
    free( sub_dirpath );
    sub_dirpath = NULL;

LBL_FAILED_SUBDIR_PATH:

LBL_FAILED_PROCESS_FILE:
    closedir( dir );
    dir = NULL;

LBL_FAILED_OPENDIR:
    return -1;
}



static int fopen_header( struct context* ctx )
{
    int r;

    ctx->header = fopen( ctx->header_path, "w" );
    if( ctx->header == NULL )
    {
        fprintf( stderr, "Failed to open header filename '%s' : %s\n", ctx->header_path, strerror(errno) );
        goto LBL_FAILED_FOPEN;
    }

    fprintf( ctx->header, "#ifndef %s\n", ctx->define );
    fprintf( ctx->header, "#define %s\n\n", ctx->define );

    fprintf( ctx->header, "#ifdef __cplusplus\n" );
    fprintf( ctx->header, "extern \"C\" {\n" );
    fprintf( ctx->header, "#endif\n\n" );


    r = fstat( fileno(ctx->header), &ctx->header_stat );
    if( r == -1 )
    {
        perror( "fstat header" );
        goto LBL_FAILED_FSTAT;
    }

    return 0;

LBL_FAILED_FSTAT:
    fclose( ctx->header);
    ctx->header= NULL;
    remove( ctx->header_filename );

LBL_FAILED_FOPEN:
    return -1;
}


static void fclose_header( struct context* ctx )
{
    fprintf( ctx->header, "\n" );
    fprintf( ctx->header, "#ifdef __cplusplus\n" );
    fprintf( ctx->header, "}\n" );
    fprintf( ctx->header, "#endif\n" );
    fprintf( ctx->header, "\n" );
    fprintf( ctx->header, "#endif\n" );

    fclose( ctx->header );
    ctx->header = NULL;
    ctx->header_path = NULL;
    ctx->header_filename = NULL;
}

static int fopen_source( struct context* ctx )
{
    int r;

    ctx->source = fopen( ctx->source_path, "w" );
    if( ctx->source == NULL )
    {
        fprintf( stderr, "Failed to open source filename '%s' : %s\n", ctx->source_path, strerror(errno) );
        goto LBL_FAILED_FOPEN;
    }

    fprintf( ctx->source, "/* Generated by dir2c */\n\n");

    r = fstat( fileno(ctx->source), &ctx->source_stat);
    if( r == -1 )
    {
        perror( "fstat header" );
        goto LBL_FAILED_FSTAT;
    }
    return 0;

LBL_FAILED_FSTAT:
    fclose( ctx->source );
    ctx->source = NULL;
    remove( ctx->source_filename );

LBL_FAILED_FOPEN:
    return -1;
}

static void fclose_source( struct context* ctx )
{
    fclose( ctx->source );
    ctx->source = NULL;
    ctx->source_filename = NULL;
    ctx->source_path = NULL;
}


// ./dir2c -t vs,fs,ini -H resources.h -s resources.c -d libimplayer_resources -p libimplayer /home/fmor/projects/timeix/fmor/libimplayer/resources

int main( int argc, char** argv )
{
    int r;
    const char* name;
    const char* extension;
    int c;

    struct context ctx = {};

    r = 0;
    name = NULL;
    extension = NULL;
    c = 0;

    // parse options
    for(;;)
    {
        c = getopt( argc, argv, "d:p:hH:s:t:");
        switch( c )
        {
            case -1:
                goto LBL_END_OPTIONS;

            case 'd':
                ctx.define = optarg;
                break;

            case 'h':
                print_help();
                exit( EXIT_SUCCESS );

            case 'H':
                ctx.header_path = optarg;
                break;

            case 's':
                ctx.source_path = optarg;
                break;

            case 'p':
                ctx.prefix = optarg;
                break;

            case 't':
                ctx.text_extension = optarg;
                break;

            case '?':
                exit( EXIT_FAILURE );

        }
    }
LBL_END_OPTIONS:
    // no params passed
    if( argc == 1 )
    {
        print_help();
        exit( EXIT_FAILURE );
    }

    // header file not specified ?
    if( ctx.header_path == NULL )
    {
        fprintf( stderr, "-H filename is required\n" );
        exit( EXIT_FAILURE );
    }
    ctx.header_filename = filename( ctx.header_path );


    // source file not specified ?
    if( ctx.source_path == NULL )
    {
        fprintf( stderr, "-s filename is required\n" );
        exit( EXIT_FAILURE );
    }
    ctx.source_filename = filename( ctx.source_path );


    // text file extension not specified ?
    if( ctx.text_extension == NULL )
        ctx.text_extension = DEFAULT_TEXT_EXTENSIONS;


    // define defined ?
    if( ctx.define == NULL )
    {
        name = filename( ctx.header_path );
        extension = file_extension( name );
        if( extension )
            ctx.define = strndup( name, extension - 1  - name );
        else
            ctx.define = strdup( name );
        if( ctx.define == NULL )
        {
            perror( "strndup" );
            exit( EXIT_FAILURE );
        }
        ctx.define_free = 1;
    }
    str_upper( ctx.define );


    // root
    r = argc - optind;
    if( r == 0 )
        ctx.root = ".";
    else if( r == 1 )
    {
        ctx.root = argv[optind];
    }
    else
    {
        fprintf( stderr, "Only one directory can be specified\n" );
        goto LBL_FAILED_ROOT;
    }


    // start
    r = fopen_header( &ctx );
    if( r != 0 )
        goto LBL_FAILED_OPEN_HEADER;

    r = fopen_source( &ctx );
    if( r != 0 )
        goto LBL_FAILED_OPEN_SOURCE;

    r = process_dir( &ctx, ctx.root, ctx.prefix );
    if( r != 0 )
        goto LBL_FAILED_PROCESS_DIR;


    fclose_source( &ctx );
    fclose_header( &ctx );
    if( ctx.define_free )
    {
        free( ctx.define );
        ctx.define = NULL;
    }
    ctx.define = NULL;
    ctx.root = NULL;
    ctx.text_extension = NULL;
    ctx.prefix = NULL;


    return EXIT_SUCCESS;

LBL_FAILED_PROCESS_DIR:
    fclose_header( &ctx );
    fclose_source( &ctx );


LBL_FAILED_OPEN_SOURCE:
    fclose( ctx.header );
    remove( ctx.header_path );
    ctx.header = NULL;
    ctx.header_filename = NULL;
    ctx.header_path = NULL;


LBL_FAILED_OPEN_HEADER:

LBL_FAILED_ROOT:
    if( ctx.define_free )
    {
        free( ctx.define );
        ctx.define = NULL;
        ctx.define_free = 0;
    }

    ctx.header_path = NULL;
    ctx.header_filename = NULL;

    ctx.source_path = NULL;
    ctx.source_filename = NULL;

    ctx.root = NULL;
    ctx.text_extension = NULL;
    ctx.prefix = NULL;

    return EXIT_FAILURE;
}

