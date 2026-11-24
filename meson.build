project('pallet_packing', 'cpp',
	version : '1.0.0',
	default_options : [
		'cpp_std=c++20',
		'buildtype=release',
		'optimization=3',
		'b_lto=true',
		'b_ndebug=true',
		'warning_level=3',
	],
)

cppCompiler = meson.get_compiler('cpp')

add_project_arguments(
	cppCompiler.get_supported_arguments('-march=native', '-funroll-loops'),
	language : 'cpp',
)

sourceFiles = files(
	'src/main.cpp',
	'src/Packing.cpp',
	'src/Greedy.cpp',
	'src/LocalSearch.cpp',
	'src/Validator.cpp',
	'src/Instances.cpp',
	'src/Experiments.cpp',
)

executable('pallet-packing', sourceFiles)
