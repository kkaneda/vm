# frontend of `system' command.
function my_system(cmd)
{
     if (show_commands) {
	  printf("%s\n", cmd) > "/dev/stderr";
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

function is_output_option(arg)
{
     return arg == "-o";
}

function is_where_to_stop_option(arg)
{
     if (match(arg, /^-[ScE]/)) return 1;
     return 0;
}

# return 1 if this option takes an argument (i.e., the next argument should 
# not be considered as an input file)
function is_take_arg_option(arg)
{
     if (arg == "-o") {
	  return 1;
     } else {
	  return 0;
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

function default_asm_output(c_path)
{
     return make_output_file_name(c_path, "s");
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
	  printf("warning: %s did not end up with an error\n", cmd) > "/dev/stderr";
     }
     return err;
}

# init variables
function init_variables()
{
# 1 if we show command lines
     show_commands = 0;

# all arguments (except for stgcc-specific ones) are accumulated
     all_args = "";

# args needed to generate object files from assembly
     assemble_args = "";

     delete xfiles;
     n_xfiles = 0;

     delete object_files;

# output file specified by "-o"
     output_file = 0;


# 1 if the next argument should be an argument to the previous flag thus
# must not be interpreted as an input file
     require_arg = 0;

     tmpfile_count = 0;
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

# this is an option (argument that begins with -)

# we get rid of -o output_file, stgcc-specific options, -E, -S, -c things,
# and -x ... option.
# types of command lines
	       if (!is_output_option(arg)) {
		    assemble_args = assemble_args " '" arg "'";
#		    assemble_args = assemble_args " " arg " ";
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

	       if (is_output_option(require_arg_flag)) {
# it is error to speicify multipe -o s
		    if (output_file) {
			 command_line_error = 1;
		    } else {
			 output_file = arg;
		    }
	       }

# we get rid of `-o filename' from all types of command lines
	       if (!is_output_option(require_arg_flag)) {
# currently we never reach here. in future, we may add an option that
# takes an argument and needs to be added to the command line
		    assemble_args = assemble_args " '" arg "'";
	       }

	       require_arg_flag = 0;
	       require_arg = 0;
	  } else {
	       xfiles[n_xfiles] = arg;
	       n_xfiles++;
	  }
     }
     return command_line_error;
}


# assemble ASM_FILE into OBJ_FILE. 
# LANG is either assembler-with-cpp or assembler.
# IN_FILE is the original input to STGCC that ASM_FILE was generated from
function assemble_file(in_file, asm_file, obj_file,
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
function assemble_file1(in_file, asm_file)
{

     assemble_file(in_file, asm_file, obj_file);
}

# assemble files generated by postprocessing
# and files specified in the command line
function assemble_files(\
			i)
{
     if(show_commands)print "assemble_files";

# assemble all assembly files in command line
     for (i = 0; i < n_xfiles; i++) {
	  asm_file = xfiles[i];	  
	  cmd = sprintf("gawk -f %s %s > %s.pp", 
			asmpp_awk, asm_file, asm_file);
	  err = my_system(cmd);
	  if (err) { return err; }
     }

     asm_files = "";
     for (i = 0; i < n_xfiles; i++) {
	  asm_files = asm_files " " xfiles[i] ".pp";
     }
     
     if (output_file) {
	  out = " -o " output_file;
     } else {
	  out = "";
     }
     
     cmd = sprintf("%s %s %s %s",
		   assembler, assemble_args, asm_files, out);
     err = my_system(cmd);
     return err;
}

# remove files (space-separated list of file names)
function remove_files(files)
{
     if (files != "") {
	  cmd = sprintf("rm -f %s", files);
	  my_system(cmd);
     } else return 0;
}

function cleanup_process(\
			 i)
{
     files = "";
     for (i = 0; i < n_xfiles; i++) {
	  files = files " " xfiles[i] ".pp";
     }
     remove_files(files);
}

function main()
{
# select compiler
     assembler = "as";
     asmpp_awk = "/home/users/kaneda/vm/trans/asmpp.awk"; # [TODO]

     init_variables();

     err = parse_args();
     if (err == 2) {
# a dangerous command line option was given, so we do not invoke gcc
	  return 1;
     } else if (err == 1) {
# we know there are errors in the command line, but we still invoke gcc
	  return invoke_error_command();
     } else {
	  result = assemble_files();
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

