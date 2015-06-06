let sym_filename = "/home/users/kaneda/vm/debug/sym/sym.txt";; 


let try_read_line  () = try
    let s = read_line  () in
    Some s
with End_of_file -> None;;

let try_input_line  chnl = try
    let s = input_line  chnl in
    Some s
with End_of_file -> None;;

          
let parse_line  s = try
    let l = Str.split (Str.regexp "[ \t]+") s in
    let addr =
	let s = List.hd l in
	Int64.of_string ("0x" ^ s) 
    in
    let sym = List.nth l 2 in
    Some (addr, sym)
with _ -> None;;


let create_sym_list  fname =
    let chnl = open_in  fname in
    let rec f  result =
	let s = try_input_line  chnl in
	match s with
	  None -> 
	    (  close_in  chnl;
	       List.rev result )
	| Some s' ->
	    let x = parse_line  s' in
	    let result' = (match x with None -> result | Some x' -> x' :: result) in
	    f  result'
    in
    f  [];;
    

let create_eip_list  () =
    let rec f  result =  
	let s = try_read_line  () in
	match s with
	  None -> List.rev  result
	| Some s' ->  
	    let result' = s' :: result in
	    f  result'
    in
    f  [];;


type t = 
    Eip of ( ( ( Int64.t * string ) * ( Int64.t * string ) ) * string * Int64.t )
  | Comment of string

let map_eip_to_sym  eip_s sl = try
    let eip = Int64.of_string  eip_s in
    let rec g  prev_sym prev_addr = function
	[] -> failwith "print_sym"
      | (addr, sym) :: t ->
	  if ((Int64.compare eip addr) < 0)
	  then Eip (((eip,eip_s), (eip,eip_s)), prev_sym, prev_addr)
	  else g sym addr t 
    in
    g "" Int64.zero sl
with _ -> Comment eip_s
      

let eips_to_string  eip1 eip2 =
    if ( Int64.compare eip1 eip2 ) = 0 
    then "  " ^ Int64.to_string eip1 
    else
	"[ " ^ 
	(Int64.to_string eip1) ^
	" - " ^
	(Int64.to_string eip2) ^
	" ]";;

let print_sym_sub  ((eip1,eip1s), (eip2,eip2s)) sym sym_addr = 
    let s = 
	"[ " ^ 
	eip1s ^
	" - " ^
	eip2s ^
	" ]" ^
	"  ====> " ^ 
	sym  ^ 
	( eips_to_string 
	    (Int64.sub eip1 sym_addr)
	    (Int64.sub eip2 sym_addr) ) ^
	"\n"
    in
    print_string s;;

let print_sym  sl el =
    let f  last_val eip_s = 
	let x = map_eip_to_sym  eip_s sl in
	match last_val with
	  Comment s ->
	    begin
		print_string ( s ^ "\n" );
		x
	    end
	| Eip ( ((e11,e11s),(e12,e12s)), sym1, sym_addr1 ) ->
	    ( match x with 
	      Comment _ -> begin 
		  print_sym_sub ( (e11,e11s), (e12,e12s) ) sym1 sym_addr1 ;
		  x;
	      end
	    | Eip  ( ( ((e21,e21s), (e22,e22s)), sym2, sym_addr2 ) ) ->
		if ( sym1 = sym2 ) 
		then 
		    let e1 = if ( Int64.compare e11 e12 ) < 0 then (e11,e11s) else (e12,e12s) and
			e2 = if ( Int64.compare e12 e22 ) > 0 then (e12,e12s) else (e22,e22s) in
		    Eip ( (e1, e2), sym2, sym_addr2 )
		else
		    begin
			print_sym_sub ( (e11,e11s), (e12,e12s) ) sym1 sym_addr1 ;
			x;			
		    end
	     )
    in
    let x = List.fold_left f ( Comment "" ) el in
    match x with
      Comment s -> print_string ( s ^ "\n" )
    | Eip ( ((e11,e11s),(e12,e12s)), sym1, sym_addr1 ) ->
	print_sym_sub ( (e11,e11s), (e12,e12s) ) sym1 sym_addr1 ;;


let main  () =
    let sl = create_sym_list sym_filename and
	el = create_eip_list () in
    print_sym sl el;;


let _ = main ();;
    
