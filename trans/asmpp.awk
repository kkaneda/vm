function main()
{
     common_init();
     scan_asm_file();
}

function common_init()
{
     delete prefixes;
     delete special_instrs;

     prefixes["repne"] = 1;
     prefixes["repnz"] = 1;

     prefixes["rep"] = 1;
     prefixes["repe"] = 1;
     prefixes["repz"] = 1;

     special_instrs["cpuid"] = 1;
     special_instrs["hlt"] = 1;

     special_instrs["in"] = 1;
     special_instrs["inb"] = 1;
     special_instrs["inw"] = 1;
     special_instrs["ind"] = 1;

     special_instrs["invlpg"] = 1;

     special_instrs["ins"] = 1;
     special_instrs["inl"] = 1;
     special_instrs["insb"] = 1;
     special_instrs["insw"] = 1;
     special_instrs["insl"] = 1;
     special_instrs["insd"] = 1;

     special_instrs["out"] = 1;
     special_instrs["outl"] = 1;
     special_instrs["outb"] = 1;
     special_instrs["outw"] = 1;
     special_instrs["outd"] = 1;

     special_instrs["outs"] = 1;
     special_instrs["outsb"] = 1;
     special_instrs["outsw"] = 1;
     special_instrs["outsl"] = 1
     special_instrs["outsd"] = 1;

     special_instrs["lgdt"] = 1;
     special_instrs["lidt"] = 1;
     special_instrs["lldt"] = 1;
     special_instrs["sgdt"] = 1;
     special_instrs["sidt"] = 1;
     special_instrs["sldt"] = 1;

     special_instrs["lds"] = 1;
     special_instrs["les"] = 1;
     special_instrs["lfs"] = 1;
     special_instrs["lgs"] = 1;
     special_instrs["lss"] = 1;
     special_instrs["lsl"] = 1;
 
     special_instrs["lmsw"] = 1;
     special_instrs["smsw"] = 1;

     special_instrs["ltr"] = 1;
     special_instrs["str"] = 1;

     special_instrs["popf"] = 1;
     special_instrs["popfd"] = 1;
     special_instrs["popfl"] = 1;
     special_instrs["pushf"] = 1;
     special_instrs["pushfd"] = 1;
     special_instrs["pushfl"] = 1;

     special_instrs["cld"] = 1;

     special_instrs["cli"] = 1;
     special_instrs["sti"] = 1;

     special_instrs["clts"] = 1;

     special_instrs["verr"] = 1;
     special_instrs["verw"] = 1;

     special_instrs["rdmsr"] = 1;
     special_instrs["wrmsr"] = 1;

     special_instrs["lar"] = 1;

     special_instrs["ljmp"] = 1;

     special_instrs["ljmpl"] = 1;

     special_instrs["lcall"] = 1;
     special_instrs["iret"] = 1;

#     special_instrs["lock"] = 1;
#     special_instrs["xchg"] = 1;


     delete special_registers;
     special_registers["%cs"] = 1;
     special_registers["%ds"] = 1;
     special_registers["%es"] = 1;
     special_registers["%fs"] = 1;
     special_registers["%gs"] = 1;
     special_registers["%ss"] = 1;

     special_registers["%tr"] = 1;

     special_registers["%cr0"] = 1;
     special_registers["%cr1"] = 1;
     special_registers["%cr2"] = 1;
     special_registers["%cr3"] = 1;
     special_registers["%cr4"] = 1;

     special_registers["%db0"] = 1;
     special_registers["%db1"] = 1;
     special_registers["%db2"] = 1;
     special_registers["%db3"] = 1;
     special_registers["%db4"] = 1;
     special_registers["%db5"] = 1;
     special_registers["%db6"] = 1;
     special_registers["%db7"] = 1;

}

function scan_asm_file(\
                       prefix)
{
     prefix = ""

     while(1) {
	  prefix = tokenize_line(prefix);

	  if (getline == 0) { break; }
     }
}

function tokenize_line(prefix, \
		       x, l, i, n)
{
     delete es;

     n = split($0, es, ";");
     
     for (i = 1; i <= n; i++) {

	   x = match(es[i], /[^[:space:]].*/);
	   if (x == 0) {
		l = "";
	   } else if (x > 1) {
		l = substr(es[i], RSTART);
		printf("%s", substr(es[i], 0, RSTART - 1));
	   } else {
		l = es[i];
	   }

	   {
		delete L_tokens;
		split(l, L_tokens, /[,[:blank:]]+/);

		if (L_tokens[1] in prefixes) {
		     prefix = l "; ";
		} else {
		     tokenize_line_sub(l, prefix);		     
		     prefix = "";
		     printf ((i < n)  ? ";" : "\n");
		}
	   }

     }

     return prefix;
}

function tokenize_line_sub(l, prefix, \
			   n, i, label, rest, illegal, f)
{
     delete L_tokens;

     if ((match(l, /^.+:/)) ) {
	  i = 2;
	  label = substr(l, RSTART, RLENGTH);
	  rest = substr(l, RSTART + RLENGTH, length(l) - RSTART + RLENGTH);
     } else {
	  i = 1;
	  label = "";
	  rest = l;
     }

     split(l, L_tokens, /[,[:blank:]]+/);	  
     
     f = 0;
     if ((match(L_tokens[i], /^pop/) || (match(L_tokens[i], /^push/))) &&
	 (L_tokens[i + 1] in special_registers)) {
	  f = 1;
     } else if (match(L_tokens[i], /^mov/)) {
	  if ((L_tokens[i + 1] in special_registers) ||
	      (L_tokens[i + 2] in special_registers)) {
	       f = 1;
	  }
     } else if (L_tokens[i] in special_instrs) {
	  f = 1;
     }

     if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%ds")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb800116c, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);
     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%es")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb80010a0, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);
     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%fs")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb80011b0, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);
     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%gs")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb80011f4, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%cr0")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb8001218, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%cr2")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb800122c, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%cr3")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb8001230, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i+1] == "%cr4")) {     
	     insert = "pushl %eax; pushl %ebx; movl $0xb8001238, %eax; movl (%eax), %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s pushl %s; %s; popl %s", label, L_tokens[i+2], insert, L_tokens[i+2]);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i + 2] == "%ds")) {     
	     insert_a = "pushl %eax";
	     insert_b = " %eax; movl %eax, (0xb800116c); popl %eax";
	     printf("%s %s; movl %s, %s", label, insert_a, L_tokens[i+1], insert_b);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i + 2] == "%es")) {     
	     insert_a = "pushl %eax";
	     insert_b = " %eax; movl %eax, (0xb80010a0); popl %eax";
	     printf("%s %s; movl %s, %s", label, insert_a, L_tokens[i+1], insert_b);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i + 2] == "%fs")) {     
	     insert_a = "pushl %eax";
	     insert_b = " %eax; movl %eax, (0xb80011b0); popl %eax";
	     printf("%s %s; movl %s, %s", label, insert_a, L_tokens[i+1], insert_b);

     } else if (match(L_tokens[i], /^movl/) && (L_tokens[i + 2] == "%gs")) {     
	     insert_a = "pushl %eax";
	     insert_b = " %eax; movl %eax, (0xb80011f4); popl %eax";
	     printf("%s %s; movl %s, %s", label, insert_a, L_tokens[i+1], insert_b);

     } else if (match(L_tokens[i], /^pop/) && (L_tokens[i + 1] == "%ds")) {
	     insert = "pushl %eax; movl 4(%esp), %eax; andl $0xff, %eax; movl %eax, (0xb800116c); popl %eax; addl $4, %esp";
	     printf("%s%s", label, insert);

     } else if (match(L_tokens[i], /^pop/) && (L_tokens[i + 1] == "%es")) {
	     insert = "pushl %eax; movl 4(%esp), %eax; andl $0xff, %eax; movl %eax, (0xb80010a0); popl %eax; addl $4, %esp";
	     printf("%s%s", label, insert);

     } else if (match(L_tokens[i], /^push/) && (L_tokens[i + 1] == "%ds")) {
	     insert = "pushl %eax; pushl %eax; pushl %ebx; movl $0xb800116c, %eax; movl (%eax), %ebx; andl $0xff, %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s%s", label, insert, rest);	  
   
     } else if (match(L_tokens[i], /^push/) && (L_tokens[i + 1] == "%es")) {
	     insert = "pushl %eax; pushl %eax; pushl %ebx; movl $0xb80010a0, %eax; movl (%eax), %ebx; andl $0xff, %ebx; movl %ebx, 8(%esp); popl %ebx; popl %eax";
	     printf("%s%s", label, insert, rest);	     

     } else if (L_tokens[i] == "pushfl") {
	     insert = "pushl %eax; pushl %eax; pushl %ebx; pushl %ecx; pushfl; movl (%esp), %eax; movl $0xb8001044, %ebx; movl (%ebx), %ecx; andl $0x0000cd5, %eax; andl $0xfffff32a, %ecx; orl %ecx, %eax; movl %eax, 16(%esp); popfl; popl %ecx; popl %ebx; popl %eax";

	     printf("%s%s", label, insert);
     } else if (L_tokens[i] == "popfl") {
	     insert = "pushl %eax; pushl %ebx; pushl %ecx; pushl %edx; pushfl; movl (%esp), %eax; movl $0xb8001044, %ebx; movl (%ebx), %ecx; popfl; andl $0x00000cd5, %eax; andl $0xfffff32a, %ecx; orl %ecx, %eax; movl $0xb80010ac, %ebx; movl (%ebx), %ecx; andl $0xf, %ecx; xorl %ebx, %ebx; cmpl $0, %ecx; sete %bl; decl %ebx; andl $(3 << 12), %ebx; movl %eax, %edx; andl $(3 << 12), %edx; shrl $12, %edx; cmpl %edx, %ecx; setae %cl; decl %ecx; andl $(1 << 9), %ecx; orl %ebx, %ecx; orl $(1 << 17), %ecx; andl %ecx, %eax; movl 16(%esp), %ebx; notl %ecx; andl %ecx, %ebx; orl %ebx, %eax; andl $0xffe7ffff, %eax; movl %eax, (0xb8001044); movl %eax, 16(%esp); popl %edx; popl %ecx; popl %ebx; popl %eax; popfl";
	     printf("%s%s", label, insert);	     
     } else if (L_tokens[i] == "cli") {
	     insert = "pushl %eax; pushl %ebx; pushl %ecx; pushl %edx; pushfl; movl (%esp), %eax; movl $0xb8001044, %ebx; movl (%ebx), %ecx; popfl; andl $0x00000cd5, %eax; andl $0xfffff32a, %ecx; orl %ecx, %eax; movl $0xb80010ac, %ebx; movl (%ebx), %ecx; andl $0xf, %ecx; movl %eax, %edx; andl $(3 << 12), %edx; shrl $12, %edx; xorl %ebx, %ebx; cmpl %ecx, %edx; setae %bl; decl %ebx; movl %ebx, %ecx; orl $~(1 << 9), %ecx; andl %ecx, %eax; not %ebx; orl $~(1 << 19), %ebx; andl %ebx, %eax; movl %eax, (0xb8001044); popl %edx; popl %ecx; popl %ebx; popl %eax";
	     printf("%s%s", label, insert);
     } else if (L_tokens[i] == "sti") {
 	     insert = "pushl %eax; pushl %ebx; pushl %ecx; pushl %edx; pushfl; movl (%esp), %eax; movl $0xb8001044, %ebx; movl (%ebx), %ecx; popfl; andl $0x00000cd5, %eax; andl $0xfffff32a, %ecx; orl %ecx, %eax; movl $0xb80010ac, %ebx; movl (%ebx), %ecx; andl $0xf, %ecx; movl %eax, %edx; andl $(3 << 12), %edx; shrl $12, %edx; xorl %ebx, %ebx; cmpl %ecx, %edx; setae %bl; decl %ebx; movl %ebx, %ecx; not %ebx; andl $(1 << 9), %ebx; orl %ebx, %eax; andl $(1 << 19), %ecx; orl %ecx, %eax; movl %eax, (0xb8001044); popl %edx; popl %ecx; popl %ebx; popl %eax";
 	     printf("%s%s", label, insert);
     } else if (L_tokens[i] == "cld") {
	     insert = "pushl %eax; pushl %ebx; pushl %ecx; pushfl; movl (%esp), %eax; movl $0xb8001044, %ebx; movl (%ebx), %ecx; popfl; andl $0x00000cd5, %eax; andl $0xfffff32a, %ecx; orl %ecx, %eax; andl $(~(1 << 10)), %eax; movl %eax, (0xb8001044); popl %ecx; popl %ebx; popl %eax";
	     printf("%s%s", label, insert);
     } else if (L_tokens[i] == "clts") {
	     insert = "pushl %eax; pushl %ebx; movl $0xb8001218, %eax; movl (%eax), %ebx; andl $(~(1 << 3)), %ebx; movl %ebx, (0xb8001218); popl %ebx; popl %eax";
	     printf("%s%s", label, insert);
     } else {
	     illegal = (f) ? " .byte 0x0f,0x0b; " : "";
	     printf("%s%s%s%s", label, illegal, prefix, rest);
     }


}

function print_illegal_instr()
{
      print "\t";
}

function fatal(s)
{
     printf("%s:%d: FATAL ERROR: %s\n", FILENAME, FNR, s) > "/dev/stderr";
     exit(1);
}


##
## real entry point
##

{
     if (NR == 1) {
	  main();
	  exit 0;
     } else {
	  fatal("dont call next command\n");
     }
}
