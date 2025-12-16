## cmake命令

### cmake_minimum_required

`cmake_minimum_required(VERSION <min>[...<policy_max>] [FATAL_ERROR])`

- 设置`CMAKE_MINIMUM_REQUIRED_VERSION`
- 隐式调用`cmake_policy(VERSION <min>[...<max>])`

### project

```cmake
project(<PROJECT-NAME> [<language-name>...])
project(<PROJECT-NAME>
        [VERSION <major>[.<minor>[.<patch>[.<tweak>]]]]
        [COMPAT_VERSION <major>[.<minor>[.<patch>[.<tweak>]]]]
        [DESCRIPTION <project-description-string>]
        [HOMEPAGE_URL <url-string>]
        [LANGUAGES <language-name>...])
```

设置变量

- `PROJECT_NAME`；顶层project会设置`CMAKE_PROJECT_NAME`
- `PROJECT_IS_TOP_LEVEL`
- 路径变量 `PROJECT_SOURCE_DIR` / `PROJECT_BINARY_DIR` / `<PROJECT-NAME>_SOURCE_DIR`

只有第一次调用 project() 时，CMake 会执行以下特殊操作。

1. 如果你设置了 CMAKE_TOOLCHAIN_FILE，CMake 会至少读取一次该工具链文件。
   (may include variables)
   `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER` / `CMAKE_SYSROOT`
   `CMAKE_C_FLAGS` / `CMAKE_CXX_FLAGS`
   `CMAKE_FIND_ROOT_PATH`
2. 因为 project() 会触发语言环境初始化，例如找编译器、设定平台标志等，
   所以 CMake 可能会再次读取工具链文件 以确保工具链的设置被正确应用到各语言

3. 设置目标平台相关变量
   `CMAKE_HOST_SYSTEM_NAME` / `CMAKE_SYSTEM_NAME`
   `CMAKE_SYSTEM_PROCESSOR` / `CMAKE_CROSSCOMPILING`

### add_executable

`add_executable(<name> <options>... <sources>...)`

options参数包括: `WIN32`, `MACOSX_BUNDLE`, `EXCLUDE_FROM_ALL`
默认情况, 执行对象的构造目录与add_executable命令所在目录是一致的。

别名: `add_executable(<name> ALIAS <target>)`

#### 导入executable

`add_executable(<name> IMPORTED [GLOBAL])`

声明：这个目标 <name> 是已有的可执行文件，不是我这个项目编译出来的，但我要在构建或依赖中使用它。

```cmake
add_executable(MyTool IMPORTED)

# 指定实际的可执行文件路径
set_target_properties(MyTool PROPERTIES
        IMPORTED_LOCATION /usr/local/bin/tool
)
```

搭配find_package，举例：

```cmake
find_package(Protobuf REQUIRED)
# Protobuf::protoc 是 IMPORTED 的 executable
add_custom_command(
        ...
        COMMAND Protobuf::protoc ...
)
```

#### HEADER_FILE_ONLY

如果你有一个 .cpp 文件，但你只是希望它显示在工程中，不实际参与构建，可以也设置 HEADER_FILE_ONLY：

`set_source_files_properties(dummy.cpp PROPERTIES HEADER_FILE_ONLY TRUE)`

应用场景: (1) 为了让 .h/.hpp 文件出现在IDE中. (2) 某些模板库文件是 .cpp，但不应编译
(3) 要让 IDE 展示 .inl、.tpp 等文件

### CMAKE_CXX_STANDARD / CMAKE_CXX_STANDARD_REQUIRED

作为`CXX_STANDARD` / `CXX_STANDARD_REQUIRED`的默认值，影响cmake-compile-features

- CMAKE_CXX_STANDARD 指定C++标准
- CXX_STANDARD_REQUIRED 是否严格使用我指定的C++标准版本。（默认为OFF）

推荐，对于指定目标设置标准，例如：

`target_compile_features(myapp PRIVATE cxx_std_20)`

### set

#### 普通的set命令

`set(<variable> <value>... [PARENT_SCOPE])`

设置/取消<variable>, 如果提供了<value>则设置; 没有则取消<variable>

可选的PARENT_SCOPE用于指定<variable>设置的**作用域**。将变量设置到上一级中，而不是当前作用域。
（1）每个 *CMakeLists.txt* 或者`function()`创建新的作用域。（2）`block()`也创建新的作用域
例如`set(my_var "some value" PARENT_SCOPE)`

#### Cache Entry

`set(<variable> <value>... CACHE <type> <docstring> [FORCE])`

全局缓存变量，会保存到CMakeCache.txt。用于向用户暴露配置选项。

#### Environment Variable

`set(ENV{<variable>} [<value>])`

设置当前CMake进程中的环境变量（不会影响系统环境变量），可以影响调用的自命令，比如`execute_process()`

### configure_file()

从input_file复制生产output_file, 并用CMakeLists.txt的VAR变量进行替换

```cmake
configure_file(<input> <output>
        [NO_SOURCE_PERMISSIONS | USE_SOURCE_PERMISSIONS |
        FILE_PERMISSIONS <permissions>...]
        [COPYONLY] [ESCAPE_QUOTES] [@ONLY]
        [NEWLINE_STYLE [UNIX|DOS|WIN32|LF|CRLF]])
```

- input 如果是相对路径，则基于`CMAKE_CURRENT_SOURCE_DIR`解析
- output 如果是相对路径，则基于`CMAKE_CURRENT_BINARY_DIR`
- NO_SOURCE_PERMISSIONS | USE_SOURCE_PERMISSIONS | FILE_PERMISSIONS <permissions>... 指定输出文件的权限
- COPYONLY 仅复制、不替换变量
- @ONLY 仅替换`@VAR@`，不替换`${VAR}`
- ESCAPE_QUOTES 转义双引号，如果CMake字符串中有`"`，会转义为`\"`
- NEWLINE_STYLE [UNIX | DOS | WIN32 | LF | CRLF] 设置换行风格

#### CMAKE_CURRENT_SOURCE_DIR vs CMAKE_CURRENT_BINARY_DIR

`CMAKE_CURRENT_SOURCE_DIR` 当前处理CMakeLists.txt的源代码目录
`CMAKE_CURRENT_BINARY_DIR` 当前处理CMakeLists.txt的构建目录

- 进入`add_subdirectory()`时变化

#### 变量转换替换

可替换`@VAR@`, `${VAR}`, `$CACHE{VAR}`, `$ENV{VAR}`。
另外 `#cmakedefine VAR ...` 将替换为 `#define VAR ...` 或者 `/* #undef VAR */`
以及 `#cmakedefine01 VAR` 替换为 `#define VAR 0` 或者 `#define VAR 1`

### option

`option(<variable> "<help_text>" [value])`

`#cmakedefine USE_MYMATH` 将option配置项转换为`#define`

### target_include_directories

指定`-I`选项的目录，如果是相对路径则基于`CMAKE_CURRENT_SOURCE_DIR`转换

```cmake
target_include_directories(<target> [SYSTEM] [AFTER|BEFORE]
        <INTERFACE|PUBLIC|PRIVATE> [items1...]
        [<INTERFACE|PUBLIC|PRIVATE> [items2...] ...])
```

- 指定的`target`不可以是**ALIAS target**
- `AFTER|BEFORE` 可以选择指定independent添加的位置
- `PRIVATE | PUBLIC` 将暴露`target`的`INCLUDE_DIRECTORIES`
- `INTERFACE | PUBLIC` 将暴露`target`的`INTERFACE_INCLUDE_DIRECTORIES`
- 一次修饰符号后面可以说明多个目
- `SYSTEM`指定SYSTEM选项，告诉编译器这些头文件是系统头文件路径，不应对其中的头文件触发警告。
    - 对于普通路径`-I/some/path`，系统路径`-isystem /some/path`
    - 对于Boost库，可以使用SYSTEM标识

`INCLUDE_DIRECTORIES` 是当前目标自己编译时使用 `-I` 选项

`INTERFACE_INCLUDE_DIRECTORIES` 提供目标的依赖者，在link到目标的时候使用这些头文件，
即使用`target_link_libraries(app mylib)`的目标会添加mylib的目录

include目录在构建、安装阶段通常是不同的，所以用`BUILD_INTERFACE` `INSTALL_INTERFACE`区分

- `INSTALL_INTERFACE`中也可以使用相对目录，相对目录基于*安装目录*解释。
- `BUILD_INTERACE` 中不能使用相对目录，此时无法完成目录的转换。

#### Question: 使用arena时自动包含Boost头文件

`find_package(Boost)`, `target_link_libraries(arena PUBLIC Boost::boost)`

方案一：
`target_include_directories(arena PUBLIC ${Boost_INCLUDE_DIRS})`
安装 arena 后，使用者也会看到这个 include 路径

方案二
`target_include_directories(arena PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}> $<INSTALL_INTERFACE:include>)`
在安装后的导出版本中，不会暴露 Boost 的路径，而是期望使用者自己 find_package(Boost) 并通过 target_link_libraries(app arena
Boost::boost) 添加 Boost 的 include

#### Question: arena使用Boost::asio

方案一 使用`target_link_libraries` （更推荐，但是需要Boost::asio目标存在）

```cmake
find_package(Boost REQUIRED COMPONENTS asio)
target_link_libraries(arena PUBLIC Boost::asio)
```

方案二 使用`target_include_directories`主动添加路径

```cmake
find_package(Boost REQUIRED COMPONENTS asio)

target_include_directories(arena
        PUBLIC
        $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
        $<INSTALL_INTERFACE:include>
)
```

### 生产表达式 GeneratorExpressions

在生产构建系统时解析表达式，比如区分Debug/Release、区分构建/安装

#### 基本语法

基本语法`$<condition:value>`, 如果condition成立，则值为value

```cmake
# Debug/Release区别
target_compile_definitions(myapp PRIVATE
        $<$<CONFIG:Debug>:DEBUG_MODE>
        $<$<CONFIG:Release>:NDEBUG>
)

# 有条件的链接库
target_link_libraries(myapp
        $<$<CONFIG:Debug>:libdebug>
        $<$<CONFIG:Release>:librelease>
)
```

```cmake
# 构建 vs 安装路径区别
target_include_directories(mylib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
```

- BUILD_INTERFACE 在构建阶段用CMake时生效
- 通过install()安装后被外部`find_package()`使用时有效

#### 判断目标属性

`$<TARGET_PROPERTY:target-name,property>`

#### 布尔表达式

`$<AND:$<CONFIG:Debug>,$<BOOL:${USE_FEATURE_X}>>`

以及`$<OR:...>`, `$<NOT:...>`, `$<STREQUAL:a,b>`, `$<BOOL:var>`

#### 例子

- `$<CONFIG:Debug>`    当前构建类型是否是 Debug
- `$<BOOL:VAR>`    是否定义/非空
- `$<STREQUAL:a,b>`    字符串相等判断
- `$<TARGET_EXISTS:tgt>`    目标是否存在
- `$<TARGET_PROPERTY:tgt,prop>`    读取目标属性
- `$<BUILD_INTERFACE:val>`    构建时使用 val
- `$<INSTALL_INTERFACE:val>`    安装时使用 va

### add_subdirectory()

`add_subdirectory(source_dir [binary_dir] [EXCLUDE_FROM_ALL] [SYSTEM])`

*binary_dir* 可以指定编译路径
`EXCLUDE_FROM_ALL`：表示子目录不会被加入"all"的构建目标中，在demo, tests, doc时有用
`SYSTEM`将该目录按SYSTEM模式编译, 防止三方库告警污染

### add_library()

`add_library(<name> [<type>] [EXCLUDE_FROM_ALL] <sources>...)`

- 可选项 `type`: `STATIC` / `SHARED` / `MODULE`
- 名称`name`，更加具体平台对应 `lib<name>.a` 或者 `<name>.lib`

输出路径

1. 默认时`${CMAKE_CURRENT_BINARY_DIR}`
2. `set_target_properties()`设置
    - `LIBRARY_OUTPUT_DIRECTORY` .so/.dylib
    - `ARCHIVE_OUTPUT_DIRECTORY` .a
    - `RUNTIME_OUTPUT_DIRECTORY` .exe/binary
    - `OUTPUT_NAME` 改名

#### Object Libraries

`add_library(<name> OBJECT <sources>...)`

至编译源文件，不生成库文件，供其它target使用

1. 使用`$<TARGET_OBJECTS:libname>`引用。 不能直接连接，不传递元信息，仅传递obj对象。
2. 直接引用object库时, 传递编译obj信息、元信息

使用时，在`add_executable()`或`add_library()`中，或者用`target_sources()`添加

#### Interface Libraries

`add_library(<name> INTERFACE)`
`add_library(<name> INTERFACE [EXCLUDE_FROM_ALL] <sources>...)` (version3.19)

用于传递头文件路径、编译选项、依赖等元信息

`set_property()`
`target_link_libraries(INTERFACE)`
`target_link_options(INTERFACE)`
`target_include_directories(INTERFACE)`
`target_compile_options(INTERFACE)`
`target_compile_definitions(INTERFACE)`
`target_sources(INTERFACE)`

只能被 `target_link_libraries(... INTERFACE)` 使用

#### Imported Libraries

`add_library(<name> <type> IMPORTED [GLOBAL])`

type类型：STATIC/SHARED/MODULE/UNKNOWN, OBJECT, INTERFACE

### target_link_libraries()

`target_link_libraries(<target> ... <item>... ...)`

```cmake
target_link_libraries(<target>
        <PRIVATE|PUBLIC|INTERFACE> <item>...
        [<PRIVATE|PUBLIC|INTERFACE> <item>...]...)
```

## target_xxxx

1. `target_compile_definitions()` 指定编译时 definitions, 可替 `add_compile_definitions()`
2. `target_compile_features()`  增加编译时 features，范围
    - `CMAKE_C_COMPILE_FEATURES`
    - `CMAKE_CUDA_COMPILE_FEATURES`
    - `CMAKE_CXX_COMPILE_FEATURES`  例如 "cxx_std_20"

3. `target_compile_options()`     例如 -Wall, -Wextra
4. `target_include_directory()` 可替代 `include_directories()`
5. `target_link_libraries()`    **核心方法**
6. `target_link_directories()`  可替代 `link_directories()`
7. `target_link_options()`
8. `target_precompile_headers()`
9. `target_sources()`

## Generator Expressions

条件通常使用：1) build configuration, 2) target properties, 3) platform information,
or 4) queryable info.

1. 条件表达式:
    - `$<condition:true_string>`,
    - `$<IF:condition,true_string,false_string>`,
    - `$<BOOL:string>`.
2. 逻辑操作
    - `$<AND:conditions>`
    - `$<OR:conditions>`
    - `$<NOT:condition>`
3. 基本比较
    - `$<STREQUAL:string1,string2>`
    - `$<EQUAL:value1,value2>`
    - `$<VERSION_LESS:v1,v2>`, ...
4. 字符串变换
    - `$<LOWER_CASE:string>`
    - `$<UPPER_CASE:string>`
    - `$<MAKE_C_IDENTIFIER:...>`
5. 列表操作
    - `$<IN_LIST:string,list>`
    - `$<LIST:LENGTH,list>`
    - `$<LIST:GET,list,index,...>`, ...
    - `$<LIST:JOIN,list,glue>`
    - `$<LIST:APPEND,list,item,...>`
    - `$<FILTER:list,INCLUDE|EXCLUDE,regex>`
6. 路径操作
    - `$<PATH_EQUAL:path1,path2>`
    - $<PATH:HAS_ROOT_NAME,path>, ...
7. 配置操作
    - `$<CONFIG:cfgs>`
    - `$<OUTPUT_CONFIG:...>`
8. 工具链语言
    - `$<PLATFORM_ID>`, `$<PLATFORM_ID:platform_ids>`
    - `$<C_COMPILER_VERSION>`, `$<C_COMPILER_VERSION:version>`: 1/0
    - `$<CXX_COMPILER_VERSION>`, `$<CXX_COMPILER_VERSION:version>`
    - ...
    - `$<C_COMPILER_ID>`, `$<C_COMPILER_ID:compiler_ids>`
    - ...
    - `$<COMPILE_LANGUAGE>`
    - `$<COMPILE_FEATURES:features>`
    - `$<COMPILE_ONLY:...>`
    - `$<LINK_LANGUAGE>`, `$<LINK_LANGUAGE:languages>`
9. 关于目标的表达式
10. 导出与安装
    - `$<INSTALL_INTERFACE:...>`, 对`install(EXPORT)`生效
    - `$<BUILD_INTERFACE:...>`, 对`export()`生效
    - `$<INSTALL_PREFIX>`
11. ``

## Installing and Testing

```cmake
# 第一组：安装构建产物
# 安装.so/.a/.exe, 即add_library() 或 add_executable() 创建的目标
install(TARGETS <target>... [...]) 
# 安装来自IMPORTED target的运行时依赖
#   通过find_package()引入的三方依赖, 安装时需要复制
install(IMPORTED_RUNTIME_ARTIFACTS <target>... [...])

# 第二组：安装静态资源文件
#   FILES,安装普通文件; PROGRAMS: 安装可执行脚本类文件
install({FILES | PROGRAMS} <file>... [...])
#   整个目录
install(DIRECTORY <dir>... [...])

# 第三组：安装自定义脚本or逻辑
install(SCRIPT <file> [...])    # 安装时执行脚本.cmake文件
install(CODE <code> [...])  # 安装时执行的CMake代码

# 第四组：
# 安装CMake的target导出信息，供find_package()使用
install(EXPORT <export-name> [...])
#   CMake3.24+ 安装导出包的元信息 (future CMake包功能）
install(PACKAGE_INFO <package-name> [...])

# CMake3.20+: 自动收集可执行文件的运行时依赖
install(RUNTIME_DEPENDENCY_SET <set-name> [...])
```

`install()`命令中使用的相对路基，都是相对于`CMAKE_INSTALL_PREFIX`，即安装系统的根目录变量

`install()`命令有多种调用方式，通用的选项包括
- `DESTINATION <dir>`
- `PERMISSIONS <permission>...`
- `CONFIGURATIONS <config>...`
- `COMPONET <component>`
- `EXCLUDE_FROM_ALL`
- `OPTIONAL`


**目标**
- 支持 make install / ninja install
- 允许统一安装到 /usr/local/, CMAKE_INSTALL_PREFIX, or DESTDIR
- 支持多平台（Windows/Mac/Linux）路径规范
- 支持 install(EXPORT ...)，导出 CMake 模块供下游 find_package() 使用
- 支持打包工具（如 CPack、cpack -G DEB/RPM）

**安装内容**

```cmake
add_library(mylib ...)
add_executable(mytool ...)

install(TARGETS mylib mytool
  RUNTIME DESTINATION bin       # 可执行文件
  LIBRARY DESTINATION lib       # 动态库 (.so/.dylib)
  ARCHIVE DESTINATION lib       # 静态库 (.a)
)

install(FILES mylib.h DESTINATION include)
```

```cmake
install(TARGETS mylib EXPORT mylibTargets)
install(EXPORT mylibTargets
    FILE mylibConfig.cmake
    NAMESPACE mylib::
    DESTINATION lib/cmake/mylib
)
```

### install安装INTERFACE library目标

Question: 在 install(TARGETS ${installable_libs}) 中包含 tutorial_compiler_flags
（一个 INTERFACE library），是必要的吗？为什么要安装 INTERFACE target？

Answer: 是的，在发布/导出 CMake 库时，安装 INTERFACE target 是必要的，
特别是当你希望下游项目 find_package() 后复用这些编译规则。

下游项目通过 find_package() 引入 MathFunctions 时，如果不安装 tutorial_compiler_flags，
它们就拿不到这些编译标志。


### CTest

`add_test()`添加测试用例

`enable_testing()`: 需要在最上层source目录中启用。**ctest**在最上层搜索test.
当引入**CTest**时，会自动启用

```cmake
add_test(Name Runs COMMAND Tutorial 25)
```

```cmake
add_test(Name Usage COMMAND Tutorial)
set_tests_properties(Usage
                     PROPERTIES PASS_REGULAR_EXPRESSION "Usage:.*number")
```

#### set_tests_properties

```cmake
set_tests_properties(<tests>...
                     [DIRECTORY <dir>]
                     PROPERTIES <prop1> <value1>
                     [<prop2> <value2>]...)
```

支持的PROPERTIES列表：https://cmake.org/cmake/help/latest/manual/cmake-properties.7.html#properties-on-tests


### CheckCXXSourceCompiles

检查c++是否支持某段代码: https://cmake.org/cmake/help/latest/module/CheckCXXSourceCompiles.html#module:CheckCXXSourceCompiles


### add_custom_command

`add_custom_command()` 是 CMake 提供的“构建前的自动生成机制”，常用于：
在构建目标之前，生成**源代码、头文件、资源文件、配置文件**等，确保这些生成文件成为构建的一部分。


### InstallRequiredSystemLibraries / cpack

将操作系统所需的运行时依赖（例如 Windows 的 MSVC redistributable DLL）加入安装包中。
通常在Windows下有用

```cmake
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_PACKAGE_VERSION_MAJOR "${Tutorial_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Tutorial_VERSION_MINOR}")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
include(CPack)
```

`set(CPACK_GENERATOR "TGZ")`: TGZ, ZIP, DEB, RPM, NSIS

使用
```bash
cmake ..
make
cpack
# -G 指定打包模式 -C 指定模式配置
# cpack -G ZIP -C Debug
```

```bash
cmake -S . -B build
cmake --build build
cpack --config build/CPackConfig.cmake
```

### 导出为CMake使用 / CMakePackageConfigHelpers

重点是`install(TARGETS)`命令不仅指定DESTINATION，还要指定EXPORT.

**Step1** 修改install命令，增加EXPORT
```cmake
install(TARGETS ${installable_libs}
        EXPORT MathFunctionsTargets
        DESTINATION lib)
```

**Step2** 在顶层cmake增加install关于EXPORT的命令
```cmake
install(EXPORT MathFunctionsTargets
  FILE MathFunctionsTargets.cmake
  DESTINATION lib/cmake/MathFunctions
)
```

安装时包含
- lib/cmake/MathFunctions/MathFunctionsTargets.cmake
- lib/cmake/MathFunctions/MathFunctionsTargets-noconfig.cmake

**Step2.1** 增加后在项目build目录导出中间文件
```cmake
export(EXPORT MathFunctionsTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/MathFunctionsTargets.cmake"
    )
```

有build文件
- build/MathFunctionsTargets.cmake


**Step3** 为了建立索引 MathFunctionsConfig.cmake
```cmake
include(CMakePackageConfigHelpers)
# generate the config file that includes the exports
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/MathFunctionsConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/MathFunctions"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
  )
```

build文件
- build/MathFunctionsConfig.cmake


**Step4** MathFunctionsConfigVersion.cmake

```cmake
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MathFunctionsConfigVersion.cmake"
    VERSION "${Tutorial_VERSION_MAJOR}.${Tutorial_VERSION_MINOR}"
    COMPATIBILITY AnyNewerVersion
)
```
build文件
- build/MathFunctionsConfigVersion.cmake

**Step5** 安装cmake索引文件

```cmake
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MathFunctionsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MathFunctionsConfigVersion.cmake
    DESTINATION lib/cmake/MathFunctions
    )
```
安装
- lib/cmake/MathFunctions/MathFunctionsConfig.cmake
- lib/cmake/MathFunctions/MathFunctionsConfigVersion.cmake
