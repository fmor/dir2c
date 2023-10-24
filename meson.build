project( 'file2c', 'c', version : '1.0.0' )

if get_option('buildtype').startswith( 'debug' )
  add_project_arguments( '-D__DEBUG__', language : 'c')
endif

includes = []
src = []

dir2c  = executable( 'dir2c' , [ 'src/dir2c.c'  ], include_directories : includes, install: true )

