# dir2c

Convert directory files to C const char arrays.

# Usage

```bash
dir2c [-h] [-t EXT1,EXT2,...] [-p PREFIX] [-d DEFINE] -H HEADER_FILENAME -s SOURCE_FILENAME DIR_PATH

-h                  : print this message
-t EXT1,EXT2,...    : if filenames have this extension, add a terminal NULL byte to the C constant. defaults is 'ini,txt,json,xml'
-p PREFIX           : prefix const name with PREFIX
-d DEFINE           : set header define guard to DEFINE. default is uppercase header filename without extension
-H FILENAME         : write header to FILENAME
-s FILENAME         : write source to FILENAME
DIRPATH             : the directory to process, if not specified then process current directory
```

# Todo

* compression
* per file


# Example

Current directory is your project dir and subdir resources contains the following files :

```bash
resources/
├── fonts
│   ├── fa-brands-400.ttf
│   ├── fa-regular-400.ttf
│   └── fa-solid-900.ttf
├── shaders
│   ├── basic.fs
│   ├── basic.vs
│   ├── triangle.fs
│   └── triangle.vs
└── textures
    └── damier.png

3 directories, 8 files
```


Now invoke dir2c :

```bash
dir2c -t vs,fs -p libfmorui -d libfmorui_resources -H src/resources.h -s src/resources.c resources
```

This will produce src/resources.h. the define string option ( -d ) is automaticaly uppercased, if not specified then header filename is used.

```c
#ifndef LIBFMORUI_RESOURCES
#define LIBFMORUI_RESOURCES

#ifdef __cplusplus
extern "C" {
#endif

extern const char libfmorui_shaders_basic_fs[269+1];
extern const char libfmorui_shaders_triangle_fs[145+1];
extern const char libfmorui_shaders_triangle_vs[380+1];
extern const char libfmorui_shaders_basic_vs[511+1];
extern const char libfmorui_fonts_fa_regular_400_ttf[60520];
extern const char libfmorui_fonts_fa_brands_400_ttf[181852];
extern const char libfmorui_fonts_fa_solid_900_ttf[388460];
extern const char libfmorui_textures_damier_png[425];

#ifdef __cplusplus
}
#endif

#endif
```

and src/resources.c :
```c
const char libfmorui_shaders_basic_fs[269+1] = {0x23,0x76,0x65,0x72,0x73,0x69,0x6f,0x6e,0x20,    ...
const char libfmorui_shaders_triangle_fs[145+1] = {0x23,0x76,0x65,0x72,0x73,0x69,0x6f,0x6e,0x20, ...
const char libfmorui_shaders_triangle_vs[380+1] = {0x23,0x76,0x65,0x72,0x73,0x69,0x6f,0x6e,0x20, ...
...
const char libfmorui_textures_damier_png[425] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,        ...
```



# Using with meson

Please note that even if you specify files as custom_target inputs, dir2c process the whole directory. And meson will re-run the custom target
only if you add file to the input array or modify one.

```python
resources = []
resources += 'resources/shaders/basic.vs'
resources += 'resources/shaders/basic.fs'
resources += 'resources/shaders/triangle.vs'
resources += 'resources/shaders/triangle.fs'
resources += 'resources/fonts/fa-regular-400.ttf'
resources += 'resources/fonts/fa-brands-400.ttf'
resources += 'resources/fonts/fa-solid-900.ttf'
resources += 'resources/textures/damier.png'

sources += custom_target('resources',
          input : resources,
          output : [ 'resources.h', 'resources.c' ],
          command : ['dir2c', '-t', 'vs,fs', '-p', 'libfmorui', '-d', 'LIBFMORUI_RESOURCES', '-H', '@OUTPUT0@', '-s', '@OUTPUT1@', '@CURRENT_SOURCE_DIR@/resources' ],
          build_by_default : true )
```
