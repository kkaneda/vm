# frontend of `system' command.
function my_system(cmd)
{
     if (show_commands) {
	  printf("my_system: %s\n", cmd) > "/dev/stderr";
     }

     x = system(cmd);
# I don't know what's wrong here.
# On windows NT, we must do this to flush buffered output. 
# (otherwise, output from CMD is not printed until we issue next command)
     if (show_commands) {
	  if (x == 0) {
	       printf("OK\n") > "/dev/stderr";
	  } else {
	       printf("NOT OK\n") > "/dev/stderr";
	  }
     }
     return x;
}

# 1 if FILE exists
function file_exists(file)
{
     cmd = sprintf("ls %s 2> /dev/null > /dev/null", file);
     if (my_system(cmd) == 0) return 1;
     else return 0;
}

# copy x to y. return status of the copy command (0 if success)
function copy_file(x, y)
{
     cmd = sprintf("cp %s %s", x, y);
     if (my_system(cmd) == 0) return 0;
     else return 1;
}

# return 1 if ARG should not be given to compilation phase (without linking)
# currently it is simply -lxx things.
function is_linker_option(arg)
{
     if (match(arg, /^-l.*/)) return 1;
     else if (arg == "--leave_link_file") return 1;
     else return 0;
}

function is_output_option(arg)
{
     return arg == "-o";
}

function is_language_select_option(arg)
{
     return arg == "-x";
}

function is_iwithprefix_option(arg)
{
     return arg == "-iwithprefix";
}

function is_where_to_stop_option(arg)
{
     if (match(arg, /^-[ScE]/)) return 1;
     return 0;
}

function is_edg_only_option(arg) 
{
     if (match(arg, /^---.*/)) return 1;
     return 0;
}

# return 1 if this option takes an argument (i.e., the next argument should 
# not be considered as an input file)
function is_take_arg_option(arg)
{
     if (arg == "-o" || arg == "-x" || arg == "-iwithprefix" || arg == "-isystem" || arg == "-include" ) {
	  return 1;
     } else {
	  return 0;
     }
}

# return its language type from file name
# (.cc --> C++, .c --> C etc.)
function default_language(file)
{
  if(match(file, /\.[^\.]*$/)) {
    ext = substr(file,RSTART+1);
    lang = default_language_from_ext[ext];
    if (lang) {
      return lang;
    } else {
      return "linker_input";
    }
  } else {
    return "linker_input";
  }
}

# xxx.yyy --> xxx.ext
# or xxx --> xxx.ext
function make_output_file_name(c_path, ext)
{
# remove directory portion
     match(c_path, /[^/]*$/);
     base = substr(c_path, RSTART);
     if(match(base, /\.[^\.]*$/)) {
	  return substr(base,1,RSTART) ext;
     } else {
	  return (base "." ext);
     }
}

function default_edg_output(c_path, lang)
{
     if (lang == "c" || lang == "cpp-output") {
	  ext = "int.c";
     } else if (lang == "c++" || lang == "c++-cpp-output") {
	  ext = "int.cc";
     }
     return make_output_file_name(c_path, ext);
}

function default_asm_output(c_path)
{
     return make_output_file_name(c_path, "s");
}

function default_nopp_asm_output(c_path)
{
     return make_output_file_name(c_path, "t");
}

function default_obj_output(c_path)
{
     return make_output_file_name(c_path, "o");
}

# when arguments are not right (e.g., when multiple -o are given), 
# we still invoke the underlying compiler to get the right error message
function invoke_error_command()
{
     cmd = sprintf("%s %s", compiler, all_args);
     err = my_system(cmd);
     if (!err) {
	  printf("vmgcc warning: %s did not end up with an error\n", cmd) > "/dev/stderr";
     }
     return err;
}

# init constants (tables)
function init_constants()
{
     delete default_language_from_ext;
     default_language_from_ext["c"] = "c";
     default_language_from_ext["i"] = "cpp-output";
     default_language_from_ext["ii"] = "c++-cpp-output";
     default_language_from_ext["m"] = "objective-c";
     default_language_from_ext["h"] = "c-header";
     default_language_from_ext["cc"] = "c++";
     default_language_from_ext["cxx"] = "c++";
     default_language_from_ext["cpp"] = "c++";
     default_language_from_ext["C"] = "c++";
     default_language_from_ext["s"] = "assembler";
     default_language_from_ext["S"] = "assembler-with-cpp";

     stop_after_cpp = 1;
     stop_after_compile = 2;
     stop_after_assemble = 3;
     stop_after_link = 4;

     delete supported_langs;
     supported_langs["c"] = 1;
     supported_langs["objective-c"] = 1;
     supported_langs["c++"] = 1;
     supported_langs["c-header"] = 1;
     supported_langs["cpp-output"] = 1;
     supported_langs["c++-cpp-output"] = 1;
     supported_langs["assembler"] = 1;
     supported_langs["assembler-with-cpp"] = 1;
     supported_langs["linker_input"] = 1;

     delete compiled_langs;
     compiled_langs["c"] = 1;
     compiled_langs["objective-c"] = 1;
     compiled_langs["c++"] = 1;
     compiled_langs["cpp-output"] = 1;
     compiled_langs["c++-cpp-output"] = 1;


     delete assemble_only_langs;
     assemble_only_langs["assembler"] = 1;
     assemble_only_langs["assembler-with-cpp"] = 1;

}

# init variables
function init_variables()
{
# 1 if we show command lines
     show_commands = 0;

# options that must come AFTER -lstext -lst (normally they are additional
# libraries used BY libst.a
     addlib_options = "";
# by default, the process won't be stopped until we have done linking
     where_to_stop = stop_after_link;
# set to 1 when we find any file that needs compilation into .o file
     need_compilation = 0;

# all arguments (except for stgcc-specific ones) are accumulated
     all_args = "";

# args needed to compile input files into assembly
     compile_args = "";
# args needed to generate object files from assembly
     assemble_args = "";
# args needed to link all files
     delete link_args;
     n_link_args = 0;

# files[l,i] is i th filename of language L
     delete xfiles;
     delete n_xfiles;
     for (l in supported_langs) n_xfiles[l] = 0;
     n_total_input_files = 0;

# non_postprocessed_assembly_files[C_file_name] is the name of the 
# (non postprocessed) assembly file generated from input file C_file_name
     delete non_postprocessed_assembly_files;
# postprocessed_assembly_files[C_file_name] is the name of the 
# (postprocessed) assembly file generated from input file C_file_name
     delete postprocessed_assembly_files;
# object_files[C_file_name/asm_file_name] is the name of the 
# object file generated from C_file_name/asm_file_name.
     delete object_files;

# output file specified by "-o"
     output_file = 0;

# language currently assumed
     language = "none";

# 1 if the next argument should be an argument to the previous flag thus
# must not be interpreted as an input file
     require_arg = 0;

     tmpfile_count = 0;

     is_dep = 0;
}

function min(x, y)
{
     if (x < y) return x;
     else return y;
}

# parse all arguments
# 0 : OK
# 1 : a command line error is detected, but we still should invoke 
#     gcc, so that it should display familiar diagnostic messages
# 2 : a command line error not understandable gcc is detected

function parse_args(i) # i : local variable
{
#     for (i = 0; i < ARGC; i++) {
#	  printf ("args[%d]= %s\n", i, ARGV[i]) > "/dev/stderr"
#     }
     
# set when we know there is an error in the command line
     command_line_error = 0;
     minus_appeared = 0;
     for (i = 0; i < ARGC; i++) {
	  arg = ARGV[i];
# skip arguments that come before "-"
	  if (!minus_appeared) {
	       if (arg == "-") {
		    minus_appeared = 1;
	       }
	       continue;
	  }

	  all_args = all_args " " arg;

	  if (command_line_error) {
# when command_line_error is detected we still continue parsing
# to pass all the given arguments to gcc.
	       continue;
	  }

# check if this is an option
	  if (match(arg, /^-.*/)) {
# check options that affect where to stop compilation and options specific to
# stgcc
	       if (arg == "-c") {
		    where_to_stop = min(where_to_stop, stop_after_assemble);
	       } else if (arg == "-S") {
		    where_to_stop = min(where_to_stop, stop_after_compile);
	       } else if (arg == "-E") {
		    where_to_stop = min(where_to_stop, stop_after_cpp);
	       } else if (arg == "--no_postprocess") {
		    no_postprocess = 1;
	       } else if (match(arg, /^--addlib=.*/)) {
# used in a form like this: "--addlib=sgc.a"
		    addlib_options = addlib_options " " substr(arg, length("--addlib=")+1);
	       } else if (match(arg, /^-sal.*/)) {
# used in a form like this: "-salX" (like -lX)
		    addlib_options = addlib_options " -l" substr(arg, length("-sal")+1);
	       } else if (match(arg, /^--compiler=.*/)) {
		    compiler = substr(arg, length("--compiler=")+1);
	       } else if (match(arg, /^-Wp,-MD,*/ )) {
		       is_dep = 1;
		       dep_file = substr(arg, length("-Wp,-MD,")+1);
	       }

# this is an option (argument that begins with -)

# we get rid of -o output_file, stgcc-specific options, -E, -S, -c things,
# and -x ... option.
# types of command lines
	       if (!is_output_option(arg) &&
		   !is_edg_only_option(arg) &&
		   !is_where_to_stop_option(arg) &&
		   !is_language_select_option(arg) &&
		   !is_iwithprefix_option(arg)) {
		    link_args[n_link_args] = arg;
		    n_link_args++;

# we further remove -l... things from the command line that generates
# assembly from C/C++ files	
		    if (!is_linker_option(arg)) {
			 compile_args = compile_args " '" arg "'";
			 assemble_args = assemble_args " '" arg "'";
#			 compile_args = compile_args " " arg " ";
#		 assemble_args = assemble_args " " arg " ";
		    }
	       }

# when we find things like -o tell the next iteration that the next 
# argument should not be interpreted as input file name
	       if (is_take_arg_option(arg)) {
		    require_arg = 1;
		    require_arg_flag = arg;
	       } else {
		    require_arg = 0;
	       }

	  } else if(require_arg) {
# this argument does not begin with `-', but it is still not an input file,
# because previous iteration is a flag that takes an argument (like -x 
# ... or -o ...)

# do appropriate actions according to the previous arg (-x, -o ...)
	       if (is_language_select_option(require_arg_flag)) {
		    language = arg;
	       } else if (is_iwithprefix_option(require_arg_flag)) {
		    link_args[n_link_args] = "-iwithprefix " arg;
		    n_link_args++;
#		    compile_args = compile_args " ' -iwithprefix " arg " '";
#		    assemble_args = assemble_args " ' -iwithprefix " arg " '";
		    compile_args = compile_args " -iwithprefix " arg " ";
		    assemble_args = assemble_args " -iwithprefix " arg " ";

	       } else if (is_output_option(require_arg_flag)) {
# it is error to speicify multiple -o s
		    if (output_file) {
			 command_line_error = 1;
		    } else {
			 output_file = arg;
		    }
	       }

# we get rid of `-o filename' from all types of command lines
	       if (!is_output_option(require_arg_flag) &&
		   !is_language_select_option(require_arg_flag) &&
		   !is_iwithprefix_option(require_arg_flag)) {
# currently we never reach here. in future, we may add an option that
# takes an argument and needs to be added to the command line
		    compile_args = compile_args " '" arg "'";
		    assemble_args = assemble_args " '" arg "'";
#		    compile_args = compile_args " " arg " ";
#		    assemble_args = assemble_args " " arg " ";
		    link_args[n_link_args] = arg;
		    n_link_args++;
	       }

	       require_arg_flag = 0;
	       require_arg = 0;
	  } else {
# this is an input file. record it into xfiles array and setup arguments.
	       if (language == "none") {
		    l = default_language(arg);
	       } else {
		    l = language;
	       }
	       if (l in supported_langs) {
		    # printf("l = %s, arg=%s\n", l, arg) > "/dev/stderr" ; 
# if arg is not a linker input (e.g., .c file), arg is later translated
# into generated object name. for now, we simply store it into link_args.
		    link_args[n_link_args] = arg;
		    n_link_args++;
		    if (l in compiled_langs) {
			 need_compilation = 1;
		    }
		    xfiles[l,n_xfiles[l]] = arg;
		    n_xfiles[l]++;
		    n_total_input_files++;
	       } else {
		    command_line_error = 1;
	       }
	  }
     }
# it is an error to speicify -o and multiple input files
     if (n_total_input_files > 1 && 
 	  where_to_stop != stop_after_link && output_file) {
	  command_line_error = 1;
     }

     return command_line_error;
}

function preprocess_file(lang, in_file,
			 tmp)
{
# otherwise simply invoke the underlying C compiler
     if (output_file) {
	  out = " -o " output_file;
     } else {
	  out = "";
     }
     cmd = sprintf("%s -E %s %s %s",
		   compiler, compile_args, in_file, out);

#    printf("cmd = %s\n", cmd) > "/dev/stderr";
     return my_system(cmd);
}

function preprocess_files(\
			  i, n)
{
     if(show_commands)print "preprocess_files";
     for (l in compiled_langs) {
	  n = n_xfiles[l];
	  for (i = 0; i < n; i++) {
	       err = preprocess_file(l, xfiles[l,i]);
	       if (err) return 1;
	  }
     }
     for (l in assemble_only_langs) {
	  n = n_xfiles[l];
	  for (i = 0; i < n; i++) {
	       err = preprocess_file(l, xfiles[l,i]);
	       if (err) return 1;
	  }
     }
     return 0;
}

# compile IN_FILE and generate NOPP_ASM_FILE
# LANG is a language name listed in compiled_langs

function compile_file(lang, in_file, nopp_asm_file,
		      edg_pp_file, src_file, err)
{
     if(show_commands)print "compile_file (" lang ") " in_file " => " nopp_asm_file;
# record the fact that NOPP_ASM_FILE came from IN_FILE
     non_postprocessed_assembly_files[in_file] = nopp_asm_file;

# EDG preprocess
# edg_pp_file : file generated by EDG
# src_file : file feed to gcc

     edg_pp_file = 0;
     src_file = in_file;
     out_lang = lang;

     cmd = sprintf("%s %s -S -x %s %s -o %s", 
		   compiler, 
		   compile_args, out_lang, src_file, nopp_asm_file);
     # printf("cmd = %s\n", cmd) > "/dev/stderr";
     err = my_system(cmd);

     if (edg_pp_file) {
	  cmd = sprintf("rm -f %s", edg_pp_file);
	  my_system(cmd);
     }
     return err;
}

function gen_temp_file(kw, ext)
{
     return "TMP_st_" kw "_" PID "_" tmpfile_count++ "." ext;
}

# compile every file of type LANG, no matter LANG is explicitly is given
# by -x or is implicitly determined
# LANG is either a language name listed in compiled_langs.
function compile_lang(lang,
		      i, n)
{
# compile every file whose language is explicitly specified with -x
     n = n_xfiles[lang];
     for (i = 0; i < n; i++) {
	  in_file = xfiles[lang,i];
	  if (where_to_stop == stop_after_compile) {
	       if (output_file) {
		    nopp_asm_file = output_file;
	       } else {
		    nopp_asm_file = default_asm_output(in_file);
	       }
	  } else {
	       nopp_asm_file = gen_temp_file("nopp", "s");
	  }
	  err_comp = compile_file(lang, in_file, nopp_asm_file);
	  if (err_comp) return 1;
     }
     return 0;
}

# compile every file that needs compilation
function compile_files()
{
     if(show_commands)print "compile_files";
     for (l in compiled_langs) {
	  err = compile_lang(l);
	  if (err) return 1;
     }
     return 0;
}

# assemble ASM_FILE into OBJ_FILE. 
# LANG is either assembler-with-cpp or assembler.
# IN_FILE is the original input to STGCC that ASM_FILE was generated from
function assemble_file(lang, in_file, asm_file, obj_file,
		       opts, cmd, err)
{
     if(show_commands)print "assemble_file (" lang ") " asm_file " => " obj_file;
# record the fact that OBJ_FILE is generated from ASM_FILE
     object_files[in_file] = obj_file;
     if (file_exists(asm_file)) {
	  cleanup_cmd = sprintf("rm -f %s.pp", asm_file);
	  cmd = sprintf("gawk -f %s %s > %s.pp", 
		         asmpp_awk, asm_file, asm_file);
	  if (my_system(cmd)) {
	       my_system(cleanup_cmd);
	       return err;
	  }
	  
	  cmd = sprintf("%s %s -x %s -c %s.pp -o %s",
			compiler, assemble_args, lang, asm_file, obj_file);
	  err = my_system(cmd);
	  my_system(cleanup_cmd);
	  return err;
     } else {
	  return 0;
     }
}

# determine output file name and asemble ASM_FILE
function assemble_file1(lang, in_file, asm_file)
{
     if (where_to_stop == stop_after_assemble) {
	  if (output_file) {
	       obj_file = output_file;
	  } else {
	       obj_file = default_obj_output(in_file);
	  }
     } else {
	  obj_file = gen_temp_file("obj", "o");
     }
     assemble_file(lang, in_file, asm_file, obj_file);
}

# assemble files generated by postprocessing
# and files specified in the command line
function assemble_files(\
			i, n)
{
     if(show_commands)print "assemble_files";
# assemble all files generated by postprocessing

     for (in_file in non_postprocessed_assembly_files) {
	  asm_file = non_postprocessed_assembly_files[in_file];
	  assemble_file1("assembler", in_file, asm_file);
     }

# assemble all assembly files in command line
     for (l in assemble_only_langs) {
	  n = n_xfiles[l];
	  for (i = 0; i < n; i++) {
	       asm_file = xfiles[l,i];
	       assemble_file1(l, asm_file, asm_file);
	  }
     }
}

# link all files
function link_files(i) # i : local variable
{
     if(show_commands)print "link_files";

# translate each input file into the name of .o file
     translated_args = "";
     for (i = 0; i < n_link_args; i++) {
	  arg = link_args[i];
	  if (arg in object_files) {
	       translated_args = translated_args " " object_files[arg];
	  } else {
	       translated_args = translated_args " " arg;
	  }
     }
     if (output_file) {
	  out = " -o " output_file;
     } else {
	  out = "";
     }
     cmd = sprintf("%s %s %s %s", linker, out, translated_args, addlib_options);
     return my_system(cmd);
}

# remove files (space-separated list of file names)
function remove_files(files)
{
     if (files != "") {
	  cmd = sprintf("rm -f %s", files);
	  my_system(cmd);
     } else return 0;
}

function cleanup_process()
{
     files = "";

    if (where_to_stop >= stop_after_assemble) {
# remove all temporary .spt files
	 for (in_file in non_postprocessed_assembly_files) {
	      f = non_postprocessed_assembly_files[in_file];
	      if (f != output_file) files = files " " f;
	 }
     }

     if (where_to_stop >= stop_after_link) {
# remove all temporary .o files
	  for (in_file in object_files) {
	       files = files " " object_files[in_file];
	  }
     }
     remove_files(files);
}

function do_compilation()
{
     if (show_commands) printf("do_compilation: %s\n", where_to_stop) > "/dev/stderr";
     if (where_to_stop == stop_after_cpp) {
	  return preprocess_files();
     } else {
# -S | -c | nowhere-to-stop 

	  if (need_compilation) {
# if any file needs compilatin, invoke the underlying C compiler
	       err_cl = compile_files();
	       if (err_cl) return err_cl;
	  }
# if -c or nowhere-to-stop, assemble assembly files
	  if (where_to_stop >= stop_after_assemble) {
	       err_as = assemble_files();
	       if (err_as) return err_as;
	  }

# if nowhere-to-stop, link files
	  if (where_to_stop >= stop_after_link) {
	       err_ld = link_files();
	       return err_ld;
	  }
     }
}

function main()
{
#    print "vmgcc\n" > "/dev/stderr"
# select compiler
     compiler = "gcc";
     linker = "gcc";
     asmpp_awk = "/home/users/kaneda/vm/trans/asmpp.awk"; # [TODO]

     init_constants();
     init_variables();

     err = parse_args();

     if (err == 2) {
# a dangerous command line option was given, so we do not invoke gcc
	  return 1;
     } else if (err == 1) {
# we know there are errors in the command line, but we still invoke gcc
	  return invoke_error_command();
     } else {
	  result = do_compilation();
	  if ( is_dep == 1 ) {
		  cmd = sprintf("/home/users/kaneda/vm/trans/dep %s > %s.vm_tmp", dep_file, dep_file);
#		  printf ("exec = \"%s\"\n", cmd);
		  my_system(cmd);

		  cmd = sprintf("mv %s.vm_tmp %s", dep_file, dep_file);
#		  printf ("exec = \"%s\"\n", cmd);
		  my_system(cmd);
	  }
	  cleanup_process(); 
	  return result;
     }
}

{
     if (NR == 1) {
	  r = main(); 
	  exit r;
     } else {
	  printf("do not use 'next'; ") > "/dev/stderr";
	  printf("everything must be done in a main function\n") > "/dev/stderr";
	  exit 1;
     }
}

