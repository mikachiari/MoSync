/* Copyright (C) 2009 Mobile Sorcery AB

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
*/

//*********************************************************************************************
//				       			MoSync Code Tools API
//*********************************************************************************************

#include "compile.h"

//****************************************
//			Build Fetch Table
//****************************************

#undef INSTR_MACRO
#define INSTR_MACRO(op_far, op_num, op_str, op_jstr, op_fetch, op_extra)	op_fetch,

int OpcodeFetch[256] =
{
	#include "InstTable.h"
};

//****************************************
// 		Build Opcode Strings Table
//****************************************

#undef INSTR_MACRO
#define INSTR_MACRO(op_far, op_num, op_str, op_jstr ,op_fetch, op_extra)	op_str,

char *OpcodeStrings[256] =
{
	#include "InstTable.h"
};

//****************************************
//		   Decode an opcode
//****************************************

#define FRED_HACK

uchar * DecodeOpcode(OpcodeInfo *thisOpcode, uchar *code_ip)
{
	uchar thisOp;
	uchar thisRD=0,thisRS=0;
	uint thisIMM=0;
	uint rip = (int) code_ip - (int) ArrayPtr(&CodeMemArray, 0);
	
	uchar *start_code_ip = code_ip;		// Make a copy
	
	int farflag = 0;
	int flags;

	CaseRef = 0;

	thisOp = *code_ip++;

	if (thisOp > _FAR)
		return 0;

	// Decode the op
	
	if (thisOp == _FAR)
	{
		thisOp = *code_ip++;
		farflag = 1;
	}

	if (thisOp >= _FAR)
		return 0;
	
	flags = OpcodeFetch[thisOp];
		
	// Fetch register info first
	
	if (flags & fetch_d)
		thisRD = *code_ip++;

	if (flags & fetch_s)
		thisRS = *code_ip++;

	// rest is either or

	if (flags & fetch_a)
	{
		if (farflag)
		{
			thisIMM = (*code_ip++ << 16);
			thisIMM += (*code_ip++ << 8);
			thisIMM += *code_ip++;
		}
		else
		{
			thisIMM = *code_ip++ << 8;
			thisIMM += *code_ip++;
		}
	}

	if (flags & fetch_c)
	{
		thisIMM = (*code_ip++ << 16);
		thisIMM += (*code_ip++ << 8);
		thisIMM += *code_ip++;
	}

	if (flags & (fetch_j | fetch_k))
		thisIMM = *code_ip++;

	if (flags & fetch_i)
	{
		thisIMM = *code_ip++;
		
		if(thisIMM > 127)
		{
			thisIMM = ((thisIMM & 127) << 8) + *code_ip++;
		}
	}

	if (flags & fetch_i)
		thisIMM = GetVarPoolEntry(thisIMM);
	
	// ** Special case for SysCalls **
	// which marks that they use R14
	
	if (thisOp == _SYSCALL)
	{
		flags |= fetch_d;
		thisRD = REG_r14;
	}

	// Save info

	thisOpcode->flags	= flags;
	thisOpcode->farflag	= farflag;
	thisOpcode->op		= thisOp;
	thisOpcode->rd		= thisRD;
	thisOpcode->rs		= thisRS;
	thisOpcode->imm		= thisIMM;
	thisOpcode->rip		= rip;
	thisOpcode->str		= OpcodeStrings[thisOp];
	thisOpcode->len		= code_ip - start_code_ip;
	
	return code_ip;
}

//****************************************
//		   Decode an opcode
//****************************************

int DecodeOpcodeIP(OpcodeInfo *thisOpcode, int code_ip)
{
	uchar *ip = (uchar *) ArrayPtr(&CodeMemArray, code_ip);
	uchar *sip = ip;

	ip = DecodeOpcode(thisOpcode, ip);
	
	code_ip += (int) (ip - sip);
	return code_ip;
}

//***************************************
//        print to disassembler
//***************************************

void DecodeAsmEmit(char *out, char *Template, ...)
{
		char 	Str[1280];
		va_list args;

		va_start(args,Template);
		vsprintf(Str,Template,args);
		va_end(args);

		strcat(out,Str);
}

//****************************************
//			Get Reg Name
//****************************************

char * DecodeRegName(int reg, int use_zero)
{
	regstr[0] = 0;

	if (reg & 0xffffff00)
		return regstr;

	if (use_zero && reg == 0)
	{
		sprintf(regstr, "#0");
		return regstr;
	}

	if (reg < 32)
	{
		strcpy(regstr, regsyms[reg]);
		return regstr;
	}

	sprintf(regstr, "#0x%x", ConstTable[reg] );
	return regstr;
}

//****************************************
//		Disassemble Case
//****************************************

void DecodeCase(char *out, OpcodeInfo *thisOpcode)
{
	SYMBOL *ref, *labref;
	int addr;
	int data_ip;

	data_ip = thisOpcode->imm;

	// Write start

	DecodeAsmEmit(out, "#0x%x,", GetDataMemLong(data_ip));
	data_ip++;

	// Write len

	DecodeAsmEmit(out, "#0x%x,", GetDataMemLong(data_ip));
	data_ip++;

	// Write table address

	ref = (SYMBOL *) ArrayGet(&DataAccessArray, thisOpcode->rip);
			
	if (!ref)
		ErrorOnIP(Error_Fatal,thisOpcode->rip, "Bad case table reference");

	CaseRef = ref;
	
	DecodeAsmEmit(out, "#%s_%d,", ref->Name, ref->LocalScope);

	// Write default address
	
	ref = (SYMBOL *) ArrayGet(&CallArray, thisOpcode->rip);
	
	// Check if what it points to in lablearry and use that
			
	if (!ref)
		ErrorOnIP(Error_Fatal,thisOpcode->rip, "Bad default case reference");

	addr = ref->Value;
	
	labref = (SYMBOL *) ArrayGet(&CodeLabelArray, addr);

	if (!labref)
		ErrorOnIP(Error_Fatal, thisOpcode->rip, "Bad default case label reference");
	
	ref = labref;

	DecodeAsmEmit(out, "#%s_%d", ref->Name, ref->LocalScope);	
}

//****************************************
//		Disassemble to string
//****************************************

int DecodeDataAccessImm(char *out, OpcodeInfo *thisOpcode)
{
	SYMBOL *ref;
	int d=0;

	ref = (SYMBOL *) ArrayGet(&DataAccessArray, thisOpcode->rip);
			
	if (!ref)
		return 0;

	if (ref->Type == SECT_data)
		d = thisOpcode->imm - ref->Value;
	else if (ref->Type == SECT_bss)
		d = thisOpcode->imm - (ref->Value + MaxDataIP);
	else if (ref->Type == SECT_code)
	{
		// !!! BEWARE: test this code !!!

		//DecodeAsmEmit(out, ">>>");
		d = thisOpcode->imm - ref->Value;
	}
	else
		ErrorOnIP(Error_Fatal,thisOpcode->rip, "(DecodeDataAccessImm) illegal section reference %d", ref->Type);
	
	if (d == 0)
	{
		DecodeAsmEmit(out, "&%s_%d", ref->Name, ref->LocalScope);
		return 1;
	}

	if (d > 0)
	{
		if (d >= ref->EndIP)
			ErrorOnIP(Error_Warning, thisOpcode->rip, "Index '%s+%d': may be out of bounds (%d length)", ref->Name, d, ref->EndIP);
	
		DecodeAsmEmit(out, "&%s_%d+%d", ref->Name, ref->LocalScope, d);
		return 1;
	}

	//FIX ARH 2008-11-03
	// Some time we got a double negative on a index from variable
	// so now invert d so that it becomes positive
	
	d = -d;

	DecodeAsmEmit(out, "&%s_%d-%d", ref->Name, ref->LocalScope, d);

	ErrorOnIP(Error_Warning, thisOpcode->rip, "Index '%s-%d': negative index may be out of bounds (%d length)", ref->Name, d, ref->EndIP);
	return 1;
}

//****************************************
//		Disassemble to string
//****************************************

int DecodeIndexAccessImm(char *out, OpcodeInfo *thisOpcode)
{
	SYMBOL *ref;
	int d=0;

	ref = (SYMBOL *) ArrayGet(&DataAccessArray, thisOpcode->rip);
			
	if (!ref)
		return 0;

	if (ref->Type == SECT_data)
		d = thisOpcode->imm - ref->Value;
	else if (ref->Type == SECT_bss)
		d = thisOpcode->imm - (ref->Value + MaxDataIP);
	else
		ErrorOnIP(Error_Fatal,thisOpcode->rip,"(DecodeDataAccessImm) illegal section reference");
	
	if (d == 0)
	{
		DecodeAsmEmit(out, "%s_%d", ref->Name, ref->LocalScope);
		return 1;
	}

	if (d > 0)
	{
		if (d >= ref->EndIP)
			ErrorOnIP(Error_Warning, thisOpcode->rip, "Index '%s+%d': may be out of bounds (%d length)", ref->Name, d, ref->EndIP);
	
		DecodeAsmEmit(out, "%s_%d+%d", ref->Name, ref->LocalScope, d);
		return 1;
	}

	//FIX ARH 2008-11-03
	// Some time we got a double negative on a index from variable
	// so now invert d so that it becomes positive
	
	d = -d;

	DecodeAsmEmit(out, "%s_%d-%d", ref->Name, ref->LocalScope, d);

	ErrorOnIP(Error_Warning, thisOpcode->rip, "Index '%s-%d': negative index may be out of bounds (%d length)", ref->Name, d, ref->EndIP);
	return 1;
}


//****************************************

int DecodeDataAccess(char *out, OpcodeInfo *thisOpcode)
{
	SYMBOL *ref;

	ref = (SYMBOL *) ArrayGet(&DataAccessArray, thisOpcode->rip);
			
	if (!ref)
		return 0;

	DecodeAsmEmit(out, "&%s_%d", ref->Name, ref->LocalScope);
	return 1;
}

//****************************************

int DecodeCallArray(char *out, OpcodeInfo *thisOpcode)
{
	SYMBOL *ref, *labref;
	int addr;
	
	ref = (SYMBOL *) ArrayGet(&CallArray, thisOpcode->rip);
	
	// !! Check if what it points to in lablearry and use that !!
			
	if (!ref)
		return 0;

	addr = ref->Value;
	
	labref = (SYMBOL *) ArrayGet(&CodeLabelArray, addr);

	if (!labref)
		return 0;
	
	ref = labref;

	DecodeAsmEmit(out, "&%s_%d", ref->Name, ref->LocalScope);
	return 1;
}

//****************************************
//		Disassemble to string
//****************************************

void DecodeAsmString(OpcodeInfo *thisOpcode, char *out)
{
	short n,len;
	char c;

	SetFilePtrFromIP(thisOpcode->rip);

	CaseRef = 0;
	out[0] = 0;

	len = strlen(thisOpcode->str);	
	
	for(n=0;n<len;n++)
	{
		c = thisOpcode->str[n];
	
		switch(c)
		{	
			case 'd': 		// Reg rd
			{
				DecodeAsmEmit(out,"%s",DecodeRegName(thisOpcode->rd, 1));
			}
			break;

			case 's': 		// Reg rs
			{
				DecodeAsmEmit(out, "%s",DecodeRegName(thisOpcode->rs, 1));
			}
			break;		

			case 'q': 		// Reg rs or DataAccess
			{
				if (DecodeDataAccess(out, thisOpcode))
					break;

				DecodeAsmEmit(out, "%s",DecodeRegName(thisOpcode->rs, 1));
			}
			break;		


			case 'i': 		// Immediate const
			case 'a': 		// Immediate address
			{
				if (DecodeCallArray(out, thisOpcode))
					break;
		
				if (DecodeDataAccessImm(out, thisOpcode))
					break;

				DecodeAsmEmit(out,"#0x%x",thisOpcode->imm);
			}
			break;

			case 'c': 		// Immediate address
				DecodeCase(out, thisOpcode);
			break;


			case 'j': 		// Immediate 8 bits
			{
				DecodeAsmEmit(out,"#0x%x",thisOpcode->imm);
			}
			break;

			case 'k': 		// Syscalls
			{
				DecodeAsmEmit(out,"%d",thisOpcode->imm);
			}		
			break;

			case 'm': 		// rs+imm	
			{

				DecodeAsmEmit(out,"%s,",DecodeRegName(thisOpcode->rs, 0));

				if (DecodeIndexAccessImm(out, thisOpcode))
					break;

				DecodeAsmEmit(out,"%d", thisOpcode->imm);

			}		
			break;

			case 'n': 		// rd+imm	
			{

				DecodeAsmEmit(out,"%s,",DecodeRegName(thisOpcode->rd, 0));

				if (DecodeIndexAccessImm(out, thisOpcode))
					break;

				DecodeAsmEmit(out,"%d", thisOpcode->imm);

			}		
			break;

			case 'x': 		// Push
			{
				DecodeAsmEmit(out,"%s,",DecodeRegName(thisOpcode->rd, 0));
				DecodeAsmEmit(out,"%s",DecodeRegName(thisOpcode->rd + thisOpcode->rs - 1, 0));
			}
			break;

			case 'y': 		// Pop
			{
				DecodeAsmEmit(out,"%s,", DecodeRegName(thisOpcode->rd - thisOpcode->rs + 1, 0));
				DecodeAsmEmit(out,"%s",  DecodeRegName(thisOpcode->rd, 0));
			}
			break;

			case  32: 		// Space
			case  9: 		// Space
			{
				DecodeAsmEmit(out," ");
			}
			break;

			default:
				if (isalpha(c))
					DecodeAsmEmit(out,"%c",tolower(c));
				else
					DecodeAsmEmit(out,"%c",c);

		}

	}
}

//****************************************
//		Disassemble Labels
//****************************************

void DecodeAsmLabel(int ip, char *out)
{
	SYMBOL *ref;

	out[0] = 0;
	
	ref = (SYMBOL *) ArrayGet(&CodeLabelArray, ip);

	if (!ref)
		return;

	if (ref->LabelType == label_Local)
		DecodeAsmEmit(out,"%s_%d", ref->Name, ref->LocalScope);
}

//****************************************
//		Disassemble Function
//****************************************

void DisassembleFunc(char *func)
{
	OpcodeInfo thisOp;
	SYMBOL *sym, *ref;
	uchar *ip, *ip_end, *ip_last;
	int base;
	char str[256];

	sym = GetGlobalSym(func);

	if (!sym)
	{
		printf("Symbol '%s' does not exist, or non-global\n", func);
		return;
	}

	if (sym->Type != SECT_code)
	{
		//printf("Only functions can be disassembled\n");

		DisassembleData(sym);
		return;
	}
		
	printf("Function '%s'\n\n", sym->Name);

	ip_end = (uchar *) ArrayPtr(&CodeMemArray, sym->EndIP);
	ip = (uchar *) ArrayPtr(&CodeMemArray, sym->Value);

	base	= sym->Value;

	while(1)
	{
		ip_last = ip;
		
		if (ip > ip_end)
			break;
	
		ip = DecodeOpcode(&thisOp, ip);
		DecodeAsmString(&thisOp, str);

		printf("%-4x: %-32s\t",base - sym->Value, str);
		
		// Print CallArray
		
		ref = (SYMBOL *) ArrayGet(&CallArray, base);
		
		if (ref)
			printf("<CA '%s'>", ref->Name);


		// Print DataAccessArray
		
		ref = (SYMBOL *) ArrayGet(&DataAccessArray, base);
		
		if (ref)
			printf("<DA '%s'>", ref->Name);
		

		printf("\n");

		base += (ip - ip_last);	
	}
}

//****************************************
//
//****************************************

void DisassembleData(SYMBOL *sym)
{
	SYMBOL *thisSym;

	uint ip;
	uint n, v;

	if (!sym)
		Error(Error_System, "DisassembleMemory null symbol\n");

	printf("Data '%s' Addr 0x%x Len %d\n\n", sym->Name, sym->Value, sym->EndIP);

	if (sym->Type == SECT_code)
		return;

	ip = sym->Value;

	if (sym->Type == SECT_bss)
		ip += MaxDataIP;

	// Byte search of memory

	for (n=ip;n<LabelArray.hi;n++)
	{
		printf("0x%x: %s\t", n, Hex8(GetDataMem(n)));

		// Check DataArray

		v = ArrayGet(&DataArray, n);

		if (v)
		{
			thisSym = (SYMBOL *) v;
									
			if (thisSym->Type == SECT_data)
				printf( "<DP '%s'>", thisSym->Name);

			if (thisSym->Type == SECT_code)
				printf("<FP '%s'>", thisSym->Name);
		}

		printf("\n");
		
		// Check if the data symbol is finished, i.e it bumps
		// into the next symbol
		
		if (n != ip)
		{
			v = ArrayGet(&LabelArray, n);

			if (v)
				break;
		}

		// Next ip
	}
}

//****************************************
//		Disassemble Function
//****************************************

int FunctionRegUsage(SYMBOL *sym)
{
	OpcodeInfo thisOp;
	uchar *ip, *ip_end;
	int regmask = 0;
	
	if (!sym)
		return -1;

	if (sym->Type != SECT_code)
		return -1;

	ip_end = (uchar *) ArrayPtr(&CodeMemArray, sym->EndIP);
	ip = (uchar *) ArrayPtr(&CodeMemArray, sym->Value);

	while(1)
	{		
		if (ip > ip_end)
			break;
	
		ip = DecodeOpcode(&thisOp, ip);

		// Ignor push and pop
		
		if (thisOp.op == _PUSH)
			continue;

		if (thisOp.op == _POP)
			continue;

		// Check for a dest reg
		
		if (thisOp.flags & fetch_d)
			if (thisOp.rd < 32)
				regmask |= (1 << thisOp.rd);

		// Check for a source reg
		
		if (thisOp.flags & fetch_s)
			if (thisOp.rs < 32)
				regmask |= (1 << thisOp.rs);
	}

	return regmask;
}


//****************************************
//		Analyse Function
//****************************************

#define REGBIT(bit) (1 << (bit))

int FunctionRegAnalyse(SYMBOL *sym)
{
	OpcodeInfo thisOp;
	uchar *ip, *ip_end;
	int params,n;

	int reg_used_as_src = 0;
	int reg_used_as_dst = 0;
	int reg_assigned = 0;
	int reg_uninit = 0;
	
	// Make sure we have a valid symbol
	
	if (!sym)
		return -1;

	// Make sure the symbol is a function

	if (sym->Type != SECT_code)
		return -1;

	// Get the start and end of the function in the code array

	ip_end = (uchar *) ArrayPtr(&CodeMemArray, sym->EndIP);
	ip = (uchar *) ArrayPtr(&CodeMemArray, sym->Value);

	// Set up the parameter regs
	
	params = sym->Params;
	
	for (n=0;n<params;n++)
	{
		reg_assigned |=  REGBIT(REG_i0 + n);
	}
	
	// Scan the function

	printf("\n");

	while(1)
	{		
		if (ip > ip_end)
			break;

		{
			char buf[2560];
			buf[0] = 0;
			DisassembleFromSource(ip - CodeMemArray.array, buf);
			printf("%s\n", buf);
		}
	
		ip = DecodeOpcode(&thisOp, ip);

		// Ignor push and pop
		
		if (thisOp.op == _PUSH)
			continue;

		if (thisOp.op == _POP)
			continue;

		// Check for a dest reg
		
		if (thisOp.flags & fetch_d)
		{
			if (thisOp.rd < 32)
			{
				// Since this is a dst regs we say its initialized
				reg_assigned |=  REGBIT(thisOp.rd);
	
				// Say this reg was used as a dst reg
				reg_used_as_dst |= REGBIT(thisOp.rd);
			}
		}
		
		// Check for a source reg
		
		if (thisOp.flags & fetch_s)
		{
			if (thisOp.rs < 32)
			{
				// check if this reg was assigned previously, if it was'nt it is uninitialized before use
			
				int is_assigned = reg_assigned & REGBIT(thisOp.rs);
			
				if (!is_assigned)
					reg_uninit |= REGBIT(thisOp.rs);
			
				reg_used_as_src |= REGBIT(thisOp.rs);
			}
		}

		//-----------------------------------------
		//		Deal with immediate calls
		//-----------------------------------------
		
		// Add the call parameters to the used list

		if (thisOp.op == _CALLI)
		{
			SYMBOL *CallSym;
			
			CallSym = (SYMBOL *) ArrayGet(&CallArray, thisOp.rip);

			if (CallSym)
			{
				int p;
				params = CallSym->Params;
	
				for (p=0;p<params;p++)
				{
					// check if this reg was assigned previously, if it was'nt it is uninitialized before use
			
					int is_assigned = reg_assigned & REGBIT(REG_i0 + p);
			
					if (!is_assigned)
						reg_uninit |= REGBIT(REG_i0 + p);
			
					reg_used_as_src |= REGBIT(REG_i0 + p);
				}

				// Check what the function returns
				// Do nothing for void
				
				if (CallSym->RetType == RET_int   ||
					CallSym->RetType == RET_float ||
					CallSym->RetType == RET_int)
					reg_used_as_dst |= REGBIT(REG_r14);

				if (CallSym->RetType == RET_double)
					reg_used_as_dst |= REGBIT(REG_r14) | REGBIT(REG_r15);
			}

		}

		//-----------------------------------------
		//		Deal with register calls
		//-----------------------------------------
		
		// Add the call parameters to the used list

		if (thisOp.op == _CALL)
		{
				int p;
	
				for (p=0;p<4;p++)
				{
					// check if this reg was assigned previously, if it was'nt it is uninitialized before use
			
					int is_assigned = reg_assigned & REGBIT(REG_i0 + p);
			
					if (!is_assigned)
						reg_uninit |= REGBIT(REG_i0 + p);
			
					reg_used_as_src |= REGBIT(REG_i0 + p);
				}

				reg_used_as_dst |= REGBIT(REG_r14) | REGBIT(REG_r15);
		}

	}

	printf("\n");

/*
	int reg_used_as_src = 0;
	int reg_used_as_dst = 0;
	int reg_assigned = 0;
	int reg_uninit = 0;
*/

	printf("                  rrrrrrrrrrrrrrrriiiiddddddddfrsz\n");
	printf("                  fedcba9876543210321076543210rtpr\n");

	printf("reg_used_as_src = %s\n", Bin32(reg_used_as_src));
	printf("reg_used_as_dst = %s\n", Bin32(reg_used_as_dst));
	printf("reg_assigned    = %s\n", Bin32(reg_assigned));
	printf("reg_uninit      = %s\n", Bin32(reg_uninit));

	if (reg_uninit)
		printf("");

	return 0;
}



//****************************************
// Enumerate the labels of a Function
//****************************************

void EnumerateFunctionLabels(SYMBOL *sym)
{
	SYMBOL *ref;
	
	uchar *ip, *ip_end;
	
	int real_ip;
	int Count;
	
	if (!sym)
		return;

	ip_end = (uchar *) ArrayPtr(&CodeMemArray, sym->EndIP);
	ip = (uchar *) ArrayPtr(&CodeMemArray, sym->Value);	
	
	real_ip	= sym->Value;
	Count = 1;
		
	while(1)
	{		
		if (ip > ip_end)
			break;

		// Print labels
		
		ref = (SYMBOL *) ArrayGet(&CodeLabelArray, real_ip);

		if (ref)
		{
			if (ref->LabelType == label_Local)
			{
				// Set the enumerator for this label

				ref->LabelEnum = Count;

				Count++;
			}
		}

		real_ip++;	
		ip++;
	}

	Count--;
	
	if (Count > MaxEnumLabel)
		MaxEnumLabel = Count;
}

//****************************************
//Output disassembly from source file line
//****************************************

int DisassembleFromSource(int codeIP, char *out)
{
	char *AsmChars = (char *) ArrayGet(&AsmCharArray, codeIP);
	char c;
	int n;
	
	if (!AsmChars)
		return 0;
	
	for (n=0;n<128;n++)
	{
		c = *AsmChars++;
		
		if (c == 0)
			break;

		if (c == 13)
			break;

		if (c == 10)
			break;

		if (c == ';')
			break;

		if (c == '/')
			if (*AsmChars == '/')
				break;

		DecodeAsmEmit(out,"%c",c);
	}
	
	return n;
}

//****************************************
//Disassemble from data in source file
//****************************************

int DisassembleDataFromSource(int dataIP, char *out)
{
	char *AsmChars = (char *) ArrayGet(&AsmCharDataArray, dataIP);

	char c;
	int n;
	
	if (!AsmChars)
		return 0;
	
	for (n=0;n<128;n++)
	{
		c = *AsmChars++;
		
		if (c == 0)
			break;

		if (c == 13)
			break;

		if (c == 10)
			break;

		if (c == ';')
			break;

		if (c == '/')
			if (*AsmChars == '/')
				break;

		DecodeAsmEmit(out,"%c",c);
	}
	
	return n;
}

//****************************************
//	Set FilePtr from Code IP address
//****************************************

int SetFilePtrFromIP(int ip)
{
	char *AsmChars = (char *) ArrayGet(&AsmCharArray, ip);

	if (AsmChars)
		FilePtr = AsmChars;
		
	return (int) AsmChars;
}
