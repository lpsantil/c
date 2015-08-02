#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "ds/ds.h"
#include "cc/c.h"

static void emitstmt(Node *n);

static FILE *o;

static void
out(char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	if(vfprintf(o, fmt, va) < 0)
		errorf("Error printing\n");
	va_end(va);
}

static void
emitfunc(Node *f)
{
	Vec *v;
	int i;
	out(".text\n");
	out(".globl %s\n", f->Func.name);
	out("%s:\n", f->Func.name);
	out("pushq %%rbp\n");
	out("movq %%rsp, %%rbp\n");
	v = f->Func.body->Block.stmts;
	for(i = 0; i < v->len; i++)
		emitstmt(vecget(v, i));
	out("leave\n");
	out("ret\n");
}

static void
emitreturn(Node *r)
{
	emitstmt(r->Return.expr);
	out("leave\n");
	out("ret\n");
}

static void
emitassign(Node *l, Node *r)
{
	Sym *sym;

	emitstmt(r);
	out("movq %%rax, %%rbx\n");
	switch(l->t) {
	case NIDENT:
		sym = l->Ident.sym;
		switch(sym->sclass) {
		case SCSTATIC:
		case SCGLOBAL:
			out("leaq %s(%%rip), %%rax\n", sym->label);
			out("movq %%rbx, (%%rax)\n");
			break;
		case SCAUTO:
			out("leaq %d(%%rbp), %%rax\n", sym->offset);
			out("movq %%rbx, (%%rax)\n");
			break;
		}
		return;
	}
	errorf("unimplemented emit assign\n");
}

static void
emitbinop(Node *n)
{
	switch(n->Binop.op) {
	case '=':
		emitassign(n->Binop.l, n->Binop.r);
		return;
	default:
		errorf("unimplemented binop\n");
	}
}

static void
emitident(Node *n)
{
	Sym *sym;

	sym = n->Ident.sym;
	switch(sym->sclass) {
	case SCSTATIC:
	case SCGLOBAL:
		out("leaq %s(%%rip), %%rax\n", sym->label);
		out("movq (%%rax), %%rax\n");
		return;
	case SCAUTO:
		out("leaq %d(%%rbp), %%rax\n", sym->offset);
		out("movq (%%rax), %%rax\n");
		return;
	}
	errorf("unimplemented ident\n");
}

static void
emitif(Node *n)
{
	emitstmt(n->If.expr);
	out("test %%rax, %%rax\n");
	out("jz %s\n", n->If.lelse);
	emitstmt(n->If.iftrue);
	out("%s:\n", n->If.lelse);
	if(n->If.iffalse)
		emitstmt(n->If.iffalse);
}

static void
emitblock(Node *n)
{
	Vec *v;
	int i;

	v = n->Block.stmts;
	for(i = 0; i < v->len ; i++)
		emitstmt(vecget(v, i));
}

static void
emitstmt(Node *n)
{
	switch(n->t){
	case NDECL:
		/* TODO */
		return;
	case NRETURN:
		emitreturn(n);
		return;
	case NNUM:
		out("movq $%s, %%rax\n", n->Num.v);
		return;
	case NIDENT:
		emitident(n);
		return;
	case NBINOP:
		emitbinop(n);
		return;
	case NIF:
		emitif(n);
		return;
	case NBLOCK:
		emitblock(n);
		return;
	default:
		errorf("unimplemented emit stmt %d\n", n->t);
	}	
}

static void
emitglobaldecl(Node *n)
{
	int  i;
	Sym *sym;

	if(n->Decl.sclass == SCTYPEDEF)
		return;
	if(n->Decl.sclass != SCGLOBAL && n->Decl.sclass != SCSTATIC)
		abort(); /* Invariant violated */
	for(i = 0; i < n->Decl.syms->len ; i++) {
		sym = vecget(n->Decl.syms, i);
		out(".comm %s, %d, %d\n", sym->label, 8, 8);
	}

}

static void
emitglobal(Node *n)
{
	switch(n->t){
	case NFUNC:
		emitfunc(n);
		return;
	case NDECL:
		emitglobaldecl(n);
		return;
	default:
		errorf("unimplemented emit global\n");
	}
}

void
emitinit(FILE *out)
{
	o = out;
}

void
emit(void)
{
	Node *n;

	while(1) {
		n = parsenext();
		if(!n)
			return;
		emitglobal(n);
	}
}
