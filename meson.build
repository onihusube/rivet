project('rivet', 'cpp', default_options : ['warning_level=3', 'cpp_std=c++20'] , meson_version : '>=0.58.0')

# デバッグ用設定
if get_option('buildtype').startswith('debug')
    env = environment()
    env.set('cpp_debugstl', 'true')
    meson.add_devenv(env)
endif

#コンパイルオプション調整
cppcompiler = meson.get_compiler('cpp').get_argument_syntax()

# VSプロジェクトに編集しうるファイルを追加する

if not meson.is_subproject()

# テストプロジェクト用の設定追加
if cppcompiler == 'msvc'
    # VSプロジェクトに編集しうるファイルを追加する
    vs_files = ['include/rivet.hpp']
    options = ['/std:c++latest', '/source-charset:utf-8', '/Zc:__cplusplus']
elif cppcompiler == 'gcc'
    vs_files = []
    options = []
endif

# Boost.UT
boostut_dep = subproject('boost.ut').get_variable('boostut_dep')

# テスト
exe = executable('rivet_test', 'test/rivet_test.cpp', include_directories : include_directories('include'), extra_files : vs_files, cpp_args : options, dependencies : [boostut_dep])
test('rivet test', exe)

else

# ライブラリ利用時の設定追加
if cppcompiler == 'msvc'
    options = ['/std:c++latest', '/source-charset:utf-8']
elif cppcompiler == 'gcc'
    options = []
endif

# subprojectとして構築時は依存オブジェクトの宣言だけしとく
chttpp_dep = declare_dependency(include_directories : include_directories('include'), cpp_args : options)

endif