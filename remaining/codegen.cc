// TODO
// loop in prologue for display thing
// find thing about - or + in offset for other things than var or param. (array for example)

#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdio.h>
#include <string.h>

#include "symtab.hh"
#include "quads.hh"
#include "codegen.hh"

using namespace std;

// Defined in main.cc.
extern bool assembler_trace;

bool HYPER_DEBUG = 0;

// Used in parser.y. Ideally the filename should be parametrized, but it's not
// _that_ important...
code_generator *code_gen = new code_generator("d.out");

// Constructor.
code_generator::code_generator(const string object_file_name)
{
    out.open(object_file_name);

    reg[RAX] = "rax";
    reg[RCX] = "rcx";
    reg[RDX] = "rdx";
}


/* Destructor. */
code_generator::~code_generator()
{
    // Make sure we close the outfile before exiting the compiler.
    out << flush;
    out.close();
}



/* This method is called from parser.y when code generation is to start.
   The argument is a quad_list representing the body of the procedure, and
   the symbol for the environment for which code is being generated. */
void code_generator::generate_assembler(quad_list *q, symbol *env)
{
    prologue(env);
    expand(q);
    epilogue(env);
}



/* This method aligns a frame size on an 8-byte boundary. Used by prologue().
 */
int code_generator::align(int frame_size)
{
    return ((frame_size + 7) / 8) * 8;
}



/* This method generates assembler code for initialisating a procedure or
   function. */
void code_generator::prologue(symbol *new_env)
{
    int ar_size;
    int label_nr;
    // Used to count parameters.
    parameter_symbol *last_arg;

    // Again, we need a safe downcast for a procedure/function.
    // Note that since we have already generated quads for the entire block
    // before we expand it to assembler, the size of the activation record
    // is known here (ar_size).
    if (new_env->tag == SYM_PROC) {
        procedure_symbol *proc = new_env->get_procedure_symbol();
        ar_size = align(proc->ar_size);
        label_nr = proc->label_nr;
        last_arg = proc->last_parameter;
    } else if (new_env->tag == SYM_FUNC) {
        function_symbol *func = new_env->get_function_symbol();
        /* Make sure ar_size is a multiple of eight */
        ar_size = align(func->ar_size);
        label_nr = func->label_nr;
        last_arg = func->last_parameter;
    } else {
        fatal("code_generator::prologue() called for non-proc/func");
        return;
    }

    /* Print out the label number (a SYM_PROC/ SYM_FUNC attribute) */
    out << "L" << label_nr << ":" << "\t\t\t" << "# " << 
        /* Print out the function/procedure name */
        sym_tab->pool_lookup(new_env->id) << endl;

    if (assembler_trace) {
        out << "\t" << "# PROLOGUE (" << short_symbols << new_env
            << long_symbols << ")" << endl;
    }

    /* Your code here */


    // RSP = stack pointer
    out << "\t\t" << "push" << "\t" << "rbp" << endl; // store the previous RBP
    out << "\t\t" << "mov" << "\t" << "rcx, rsp" << endl; // save the previous RSP in a temporary location
                                                          // this is also this frame's RBP

    // we need a loop here
	//    out << "this should be in a loop so we get all the previous RBPs" << endl;
	for (int i = 1; i <= new_env->level; ++i) {
		// display values are copied here (we think this is right)
		out << "\t\t" << "push" << "\t" << "[rbp-" << 8*i << "]" << endl;
	}
	// read stuff betwen RBP (the previous one should be the new current one after a return)
	// so between RBP and our current address (we should be at return or something?
	// out << " RBP " << rbp << endl; // runtime things!! :(
	// out << " RSP " << rsp << endl;
	
	
    // while (last_arg != NULL) {
    //  out << "\t\t" << "push" << "\t" << "[rbp-8]" << endl;

    //   last_arg = last_arg->preceding;
    // }

    out << "\t\t" << "push" << "\t" << "rcx" << endl; // push the previous RSP on the stack
    out << "\t\t" << "mov" << "\t" << "rbp, rcx" << endl; // and really make it our new RBP
    out << "\t\t" << "sub" << "\t" << "rsp, " << ar_size << endl; // allocate space for temporary storage (ar_size correct here?)

	if (HYPER_DEBUG) out << "#ending prologue" << endl;
    out << flush;
}



/* This method generates assembler code for leaving a procedure or function. */
void code_generator::epilogue(symbol *old_env)
{
    if (assembler_trace) {
        out << "\t" << "# EPILOGUE (" << short_symbols << old_env
            << long_symbols << ")" << endl;
    }

    /* Your code here */
    out << "\t\t" << "leave" << endl; // release activation record
    out << "\t\t" << "ret" << endl; // return

    out << flush;
}



/* This function finds the display register level and offset for a variable,
   array or a parameter. Note the pass-by-pointer arguments. */
void code_generator::find(sym_index sym_p, int *level, int *offset)
{
  /* Your code here */
  symbol *s = sym_tab->get_symbol(sym_p);
  *level = s->level;
  // *offset = s->offset - (*level+1)*8;
  if (s->tag == SYM_PARAM) {
	  *offset = s->offset + 16;
  } else {
	  *offset = -(s->offset) - (*level+1)*8;

  }
}

/*
 * Generates code for getting the address of a frame for the specified scope level.
 */
void code_generator::frame_address(int level, const register_type dest)
{
	if (assembler_trace) { out << "#begin frame_address" << endl; }
    /* Your code here */
   // Count and find the correct RBP for the level, something like 8*(level?) or maybe not +1
   out << "\t\t" << "mov" << "\t"  << reg[dest] << ", [rbp-" << 8*(level) << "]" << endl; 
	if (assembler_trace) { out << "#end frame_address" << endl; }
}

/* This function fetches the value of a variable or a constant into a
   register. */
void code_generator::fetch(sym_index sym_p, register_type dest)
{
    /* Your code here */
	if (assembler_trace) { out << "#begin fetch" << endl; }
  symbol *s = sym_tab->get_symbol(sym_p);
  constant_symbol* const_sym;
  variable_symbol* var_sym;
  parameter_symbol* param_sym;
  constant_value const_val;
  long i;
  double r;

  // constants have their value directly in the data strucutre.
  if (s->tag == SYM_CONST ) {
	  const_sym = s->get_constant_symbol();
	  const_val = const_sym->const_value;       
	  if (s->type == integer_type) {
		  i = const_val.ival;
	  } else if(s->type == real_type) {
		  r = const_val.rval;
		  i = sym_tab->ieee(r);
	  }
	  out << "\t\t" << "mov" << "\t"  << reg[dest] << ", " << i << endl;
  } else if (s->tag == SYM_VAR || s->tag == SYM_PARAM) {
	  int level, offset;
	  
	  find(sym_p, &level, &offset);
	  register_type tmp = RCX;
	  frame_address(level, tmp); // write frame addr to our tmp register
	  
	  if (s->tag == SYM_VAR) {
		  out << "\t\t" << "mov" << "\t"  << reg[dest] << ", " << "[" << reg[tmp] << "-" << -offset << "]" << endl; 
	  } else {
		  out << "\t\t" << "mov" << "\t"  << reg[dest] << ", " << "[" << reg[tmp] << "+" << offset << "]" << endl;	  }
    } else {
		cout << s << endl;
      fatal("fetch normal Fetch on a non variable or constant");
    }
	if (assembler_trace) { out << "#end fetch" << endl; }
}

void code_generator::fetch_float(sym_index sym_p)
{
    /* Your code here */
  symbol *s = sym_tab->get_symbol(sym_p);
  constant_symbol* const_sym;
  variable_symbol* var_sym;
  parameter_symbol* param_sym;
  constant_value const_val;
  long i;
  double r;

  // constants have their value directly in the data strucutre.
  if (s->tag == SYM_CONST) {
	  const_sym = s->get_constant_symbol();
	  const_val = const_sym->const_value;       
	  if (s->type == integer_type) {
		  return;
	  } else if(s->type == real_type) {
		  r = const_val.rval;
		  i = sym_tab->ieee(r);
	  }
	  out << "\t\t" << "mov" << "\t" << "rax, " << i << endl; 
	  out << "\t\t" << "push" << "\t" << "rax" << endl; 
	  out << "\t\t" << "fld" << "\t" << "qword ptr [rsp]" << endl;
	  out << "\t\t" << "add" << "\t" << "rsp, 8" << endl;
	  return;
  }
    if (s->tag == SYM_VAR || s->tag == SYM_PARAM) {
	  int level, offset;
	  
	  find(sym_p, &level, &offset);
	  register_type tmp = RCX ;
	  frame_address(level, tmp); // write frame addr to our tmp register

	  if (s->tag == SYM_VAR) {
		  out << "\t\t" << "fld" << "\t" <<  "qword ptr [" << reg[tmp] << "-" << -offset << "]" << endl;
	  } else {
		  out << "\t\t" << "fld" << "\t" <<  "qword ptr [" << reg[tmp] << "+" << offset << "]" << endl;
	  }
    } else {
      fatal("fetch_float Fetch on a non variable or constant");
    }
}



/* This function stores the value of a register into a variable. */
void code_generator::store(register_type src, sym_index sym_p)
{
	if (assembler_trace) { out << "#begin store" << endl; }
	  int level, offset;
	  find(sym_p, &level, &offset);
	  register_type tmp;
	  if (src != RCX) {
		  tmp = RCX ;
	  } else {
		  tmp = RAX ;
	  }
	  symbol *s = sym_tab->get_symbol(sym_p); 
	  frame_address(level, tmp); // write frame addr to our tmp register

	  if (s->tag == SYM_VAR) {
		  out << "\t\t" << "mov" << "\t" <<  "[" << reg[tmp] << "-" << -offset << "], " << reg[src] << endl;
	  } else {
		  out << "\t\t" << "mov" << "\t" <<  "[" << reg[tmp] << "+" << offset << "], " << reg[src] << endl;
	  }
	if (assembler_trace) { out << "#end store" << endl; }
}

void code_generator::store_float(sym_index sym_p)
{
    /* Your code here */
	int level, offset;
	find(sym_p, &level, &offset);

	symbol *s = sym_tab->get_symbol(sym_p);
	register_type tmp = RCX ;
	frame_address(level, tmp); // write frame addr to our tmp register
	
	if (s->tag == SYM_VAR) {
		out << "\t\t" << "fstp" << "\t" << "qword ptr [" << reg[tmp] << "-" << -offset << "]" << endl; 
	} else {
		out << "\t\t" << "fstp" << "\t" << "qword ptr [" << reg[tmp] << "+" << offset << "]" << endl; 
	}
}


/* This function fetches the base address of an array. */
void code_generator::array_address(sym_index sym_p, register_type dest)
{
    /* Your code here */
	if (assembler_trace) { out << "#begin array_address" << endl; }
	int level, offset;
	find(sym_p, &level, &offset);
	register_type tmp = RCX ;
	frame_address(level, tmp); // write frame addr to our tmp register
	  
	out << "\t\t" << "mov" << "\t" << reg[dest] << ", " << reg[tmp] << endl; 

	if (offset < 0) {
		out << "\t\t" << "sub" << "\t" << reg[dest] << ", " << -offset << endl;
	} else {
		out << "\t\t" << "add" << "\t" << reg[dest] << ", " << offset << endl;
	}
	if (assembler_trace) { out << "#end array_address" << endl; }
}

/* This method expands a quad_list into assembler code, quad for quad. */
void code_generator::expand(quad_list *q_list)
{
    long quad_nr = 0;       // Just to make debug output easier to read.
    

    // We use this iterator to loop through the quad list.
    quad_list_iterator *ql_iterator = new quad_list_iterator(q_list);

    quadruple *q = ql_iterator->get_current(); // This is the head of the list.

    while (q != NULL) {
        quad_nr++;

        // We always do labels here so that a branch doesn't miss the
        // trace code.
        if (q->op_code == q_labl) {
            out << "L" << q->int1 << ":" << endl;
        }

        // Debug output.
        if (assembler_trace) {
            out << "\t" << "# QUAD " << quad_nr << ": "
                << short_symbols << q << long_symbols << endl;
        }

        // The main switch on quad type. This is where code is actually
        // generated.
        switch (q->op_code) {
        case q_rload:
        case q_iload:
            out << "\t\t" << "mov" << "\t" << "rax, " << q->int1 << endl;
            store(RAX, q->sym3);
            break;

        case q_inot: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "je" << "\t" << "L" << label << endl;
            // Not equal branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // Equal branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" <<  endl;
            store(RAX, q->sym3);
            break;
        }
        case q_ruminus:
            fetch_float(q->sym1);
            out << "\t\t" << "fchs" << endl;
            store_float(q->sym3);
            break;

        case q_iuminus:
            fetch(q->sym1, RAX);
            out << "\t\t" << "neg" << "\t" << "rax" << endl;
            store(RAX, q->sym3);
            break;

        case q_rplus:
            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "faddp" << endl;
            store_float(q->sym3);
            break;

        case q_iplus:
            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "add" << "\t" << "rax, rcx" << endl;
            store(RAX, q->sym3);
            break;

        case q_rminus:
            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "fsubp" << endl;
            store_float(q->sym3);
            break;

        case q_iminus:
            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "sub" << "\t" << "rax, rcx" << endl;
            store(RAX, q->sym3);
            break;

        case q_ior: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jne" << "\t" << "L" << label << endl;
            fetch(q->sym2, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jne" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_iand: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "je" << "\t" << "L" << label << endl;
            fetch(q->sym2, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "je" << "\t" << "L" << label << endl;
            // True branch
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // False branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_rmult:
            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "fmulp" << endl;
            store_float(q->sym3);
            break;

        case q_imult:
            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "imul" << "\t" << "rax, rcx" << endl;
            store(RAX, q->sym3);
            break;

        case q_rdivide:
            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "fdivp" << endl;
            store_float(q->sym3);
            break;

        case q_idivide:
            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cqo" << endl;
            out << "\t\t" << "idiv" << "\t" << "rax, rcx" << endl;
            store(RAX, q->sym3);
            break;

        case q_imod:
            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cqo" << endl;
            out << "\t\t" << "idiv" << "\t" << "rax, rcx" << endl;
            store(RDX, q->sym3);
            break;

        case q_req: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "fcomip" << "\t" << "ST(0), ST(1)" << endl;
            // Clear the stack
            out << "\t\t" << "fstp" << "\t" << "ST(0)" << endl;
            out << "\t\t" << "je" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_ieq: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cmp" << "\t" << "rax, rcx" << endl;
            out << "\t\t" << "je" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_rne: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch_float(q->sym1);
            fetch_float(q->sym2);
            out << "\t\t" << "fcomip" << "\t" << "ST(0), ST(1)" << endl;
            // Clear the stack
            out << "\t\t" << "fstp" << "\t" << "ST(0)" << endl;
            out << "\t\t" << "jne" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_ine: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cmp" << "\t" << "rax, rcx" << endl;
            out << "\t\t" << "jne" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_rlt: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            // We need to push in reverse order for this to work
            fetch_float(q->sym2);
            fetch_float(q->sym1);
            out << "\t\t" << "fcomip" << "\t" << "ST(0), ST(1)" << endl;
            // Clear the stack
            out << "\t\t" << "fstp" << "\t" << "ST(0)" << endl;
            out << "\t\t" << "jb" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_ilt: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cmp" << "\t" << "rax, rcx" << endl;
            out << "\t\t" << "jl" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_rgt: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            // We need to push in reverse order for this to work
            fetch_float(q->sym2);
            fetch_float(q->sym1);
            out << "\t\t" << "fcomip" << "\t" << "ST(0), ST(1)" << endl;
            // Clear the stack
            out << "\t\t" << "fstp" << "\t" << "ST(0)" << endl;
            out << "\t\t" << "ja" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_igt: {
            int label = sym_tab->get_next_label();
            int label2 = sym_tab->get_next_label();

            fetch(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "cmp" << "\t" << "rax, rcx" << endl;
            out << "\t\t" << "jg" << "\t" << "L" << label << endl;
            // False branch
            out << "\t\t" << "mov" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "jmp" << "\t" << "L" << label2 << endl;
            // True branch
            out << "\t\t" << "L" << label << ":" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, 1" << endl;

            out << "\t\t" << "L" << label2 << ":" << endl;
            store(RAX, q->sym3);
            break;
        }
        case q_rstore:
        case q_istore:
            fetch(q->sym1, RAX);
            fetch(q->sym3, RCX);
            out << "\t\t" << "mov" << "\t" << "[rcx], rax" << endl;
            break;

        case q_rassign:
        case q_iassign:
            fetch(q->sym1, RAX);
            store(RAX, q->sym3);
            break;

        case q_param: {
            /* Your code here */
			if (HYPER_DEBUG) out << "# q_param begin" << endl;
            fetch(q->sym1, RAX);
			out << "\t\t" << "push" << "\t" << "rax" << endl;
            break;
		}
        case q_call: {
			if (HYPER_DEBUG) out << "# q_call begin" << endl;
			// Used to count parameters.
			parameter_symbol *last_arg;

            /* Your code here */
			//			fetch(q->sym1, RAX); // what to call
			//fetch(q->int1, RAX); // what to call
			//            fetch(q->int2, RCX); //no. param
			//			fetch(q->sym3, RDX); // return addr
			symbol *s = sym_tab->get_symbol(q->sym1);
			int label_nr;
			if (s->tag == SYM_PROC) {
				procedure_symbol *proc = s->get_procedure_symbol();
				label_nr = proc->label_nr;
			} else if (s->tag == SYM_FUNC) {
				function_symbol *func = s->get_function_symbol();
				label_nr = func->label_nr;
			} else {
				fatal("code_generator::expand() for q_call called for non-proc/func");
				return;
			}
			if (q->sym3 == NULL_SYM) {
				out << "\t\t" << "call" << "\t" << "L" << label_nr
					<< "\t" << "# " << sym_tab->pool_lookup(s->id) << endl; // make the call

				out << "\t\t" << "add" << "\t" << "rsp, " << 8*q->int2 << endl; // store the return (in rax) to the return address
				break;
			}
			//			out << "\t\t" << "push" << "\t" << "rdx" << endl; // push ret addr to stack
			out << "\t\t" << "call" << "\t" << "L" << label_nr
				<< "\t" << "# " << sym_tab->pool_lookup(s->id) << endl; // make the call
			//			out << "\t\t" << "mov" << "\t" << "rax, [rdx]" << endl; // store
			out << "\t\t" << "add" << "\t" << "rsp, " << 8*q->int2 << endl; // store the return (in rax) to the return address
			if (s->tag == SYM_FUNC) {
				store(RAX, q->sym3);			
				// basically should do something like this
				//			out << "\t\t" << "mov" << "\t" << "[rdx], rax" << endl; // store the return (in rax) to the return address
			}

            break;
        }
        case q_rreturn:
        case q_ireturn:
            fetch(q->sym2, RAX);
            out << "\t\t" << "jmp" << "\t" << "L" << q->int1 << endl;
            break;

        case q_lindex:
            array_address(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "imul" << "\t" << "rcx, " << STACK_WIDTH << endl;
            out << "\t\t" << "sub" << "\t" << "rax, rcx" << endl;
            store(RAX, q->sym3);
            break;

        case q_rrindex:
        case q_irindex:
            array_address(q->sym1, RAX);
            fetch(q->sym2, RCX);
            out << "\t\t" << "imul" << "\t" << "rcx, " << STACK_WIDTH << endl;
            out << "\t\t" << "sub" << "\t" << "rax, rcx" << endl;
            out << "\t\t" << "mov" << "\t" << "rax, [rax]" << endl;
            store(RAX, q->sym3);
            break;

        case q_itor: {
            block_level level;      // Current scope level.
            int offset;             // Offset within current activation record.

            find(q->sym1, &level, &offset);
            frame_address(level, RCX);
            out << "\t\t" << "fild" << "\t" << "qword ptr [rcx";
            if (offset >= 0) {
                out << "+" << offset;
            } else {
                out << offset; // Implicit "-"
            }
            out << "]" << endl;
            store_float(q->sym3);
        }
        break;

        case q_jmp:
            out << "\t\t" << "jmp" << "\t" << "L" << q->int1 << endl;
            break;

        case q_jmpf:
            fetch(q->sym2, RAX);
            out << "\t\t" << "cmp" << "\t" << "rax, 0" << endl;
            out << "\t\t" << "je" << "\t" << "L" << q->int1 << endl;
            break;

        case q_labl:
            // We handled this one above already.
            break;

        case q_nop:
            // q_nop quads should never be generated.
            fatal("code_generator::expand(): q_nop quadruple produced.");
            return;
        }

        // Get the next quad from the list.
        q = ql_iterator->get_next();
    }

    // Flush the generated code to file.
    out << flush;
}
