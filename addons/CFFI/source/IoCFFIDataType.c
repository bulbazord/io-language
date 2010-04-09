//metadoc CFFIPointer copyright 2006 Trevor Fancher. All rights reserved.
//metadoc CFFIPointer license BSD revised
//metadoc CFFIPointer category Bridges
//metadoc CFFIPointer description An Io interface to C

#include "IoCFFIDataType.h"
#include "IoCFFIPointer.h"
#include "IoCFFIStructure.h"
#include "IoCFFIFunction.h"
#include "IoCFFIArray.h"
#include "IoSeq.h"
#include "IoNumber.h"
#include "IoObject.h"
#include "IoState.h"
#include <stdlib.h>
#include <string.h>
#include <ffi.h>

#define DATA(self) ((IoCFFIDataTypeData *)(IoObject_dataPointer(self)))
#define POINTER(data) ((data)->valuePointer)

void *IoCFFIDataType_null = NULL;

IoTag *IoCFFIDataType_newTag(void *state)
{
	IoTag *tag = IoTag_newWithName_("DataType");
	IoTag_state_(tag, state);
	IoTag_freeFunc_(tag, (IoTagFreeFunc *)IoCFFIDataType_free);
	IoTag_cloneFunc_(tag, (IoTagCloneFunc *)IoCFFIDataType_rawClone);
	//IoTag_markFunc_(tag, (IoTagMarkFunc *)IoCFFIDataType_mark);
	return tag;
}

IoCFFIDataType *IoCFFIDataType_proto(void *state)
{
	IoObject *self = IoObject_new(state);
	IoObject_tag_(self, IoCFFIDataType_newTag(state));

	IoObject_setDataPointer_(self, calloc(1, sizeof(IoCFFIDataTypeData)));
	memset(DATA(self), 0, sizeof(IoCFFIDataTypeData));
	DATA(self)->valuePointer = &(DATA(self)->type);

	IoState_registerProtoWithFunc_(state, self, IoCFFIDataType_proto);

	{
		IoMethodTable methodTable[] = {
			{"asBuffer", IoCFFIDataType_asBuffer},
			{"setValue", IoCFFIDataType_setValue},
			{"size", IoCFFIDataType_size},
			{"value", IoCFFIDataType_value},
			{NULL, NULL},
		};
		IoObject_addMethodTable_(self, methodTable);
	}

	return self;
}

IoCFFIDataType *IoCFFIDataType_rawClone(IoCFFIDataType *proto)
{
	IoObject *self = IoObject_rawClonePrimitive(proto);
	IoObject_setDataPointer_(self, calloc(1, sizeof(IoCFFIDataTypeData)));
	memset(DATA(self), 0, sizeof(IoCFFIDataTypeData));
	DATA(self)->valuePointer = &(DATA(self)->type);

	return self;
}

IoCFFIDataType *IoCFFIDataType_new(void *state)
{
	IoObject *proto = IoState_protoWithInitFunction_(state, IoCFFIDataType_proto);
	return IOCLONE(proto);
}

void IoCFFIDataType_free(IoCFFIDataType *self)
{
	IoCFFIDataTypeData *data;
	data = DATA(self);

	if ( data->needToFreeStr ) {
		free(data->type.str);
		data->needToFreeStr = 0;
	}
	free(DATA(self));
}

void IoCFFIDataType_mark(IoCFFIDataType *self)
{
}

/* ---------------------------------------------------------------- */

void *IoCFFIDataType_ValuePointerFromObject_(IoCFFIDataType* self, IoObject *o)
{
	IoObject *_self, *number;
	void* ret;
	char c;

	// this is a hack so macros relying on self will work
	//self = o;

	// When a type wants the value pointer of an object, it must
	// hold a reference to that object to protect it from GC.
	// If that type is getting the pointer temproraly (ie: to just
	// copy the data it points to) "self" is allowed to be NULL and
	// the type will not hold a reference to the object.
	if ( !self ) self = o, _self = NULL;
	else _self = self;

	if ( ISNUMBER(o) ) {
		number = IoState_doCString_(IoObject_state(o), "CFFI Types Double clone");
		DATA(number)->type.d = IoObject_dataDouble(o);
		return IoCFFIDataType_ValuePointerFromObject_(_self, number);
	}
	else if ( ISSEQ(o) ) {
		// There is an inconsistency when dealing with Sequences: they are used both for Char and CString types.
		// If a CString, we want the address where the string is stored (we have a double indirection here). If
		// a Char, we want the address where the char is stored (that is, a simple indirection).
		c = CSTRING(IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_setValue"))[0];
		if(c == 'c' || c == 'C')
		ret = *(void **)IoObject_dataPointer(o);
		else
		ret = (void *)IoObject_dataPointer(o);
	}
	else if ( ISNIL(o) )              ret = &IoCFFIDataType_null;
	else if ( ISCFFIDataType(o) )     ret = IoCFFIDataType_valuePointer(o);
	else if ( ISCFFIPointer(o) )      ret = IoCFFIPointer_valuePointer(o);
	else if ( ISCFFIStructure(o) )    ret = IoCFFIStructure_valuePointer(o);
	else if ( ISCFFIFunction(o) )     ret = IoCFFIFunction_valuePointer(o);
	else if ( ISCFFIArray(o) )        ret = IoCFFIArray_valuePointer(o);
	else {
		IoState_error_(IOSTATE, NULL, "unknown object to get pointer from");
		return NULL;
		//ret = o;
	}

	if ( _self )
		IoObject_setSlot_to_(_self, IOSYMBOL("_keepRef"), o);

	return ret;
}

IoCFFIDataType *IoCFFIDataType_value(IoCFFIDataType *self, IoObject *locals, IoMessage *m)
{
	return IoCFFIDataType_objectFromData_(self, IoCFFIDataType_valuePointer(self));
}

IoObject *IoCFFIDataType_setValueFromData(IoCFFIDataType *self, void *value)
{   
	char c, *cp;
	IoCFFIDataTypeData *data = NULL;

	if ( ISCFFIDataType(self) ) {
		data = DATA(self);
		if ( data->needToFreeStr ) {
			free(data->type.str);
			data->needToFreeStr = 0;
		}
	}

	c = CSTRING( IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_setValue") )[0];
	switch ( c ) {
		case 'c':
			// see comment in IoCFFIDataType_ValuePointerFromObject_()
			*(char *)POINTER(data) = *((char *)value); break;
		case 'C':
			// see comment in IoCFFIDataType_ValuePointerFromObject_()
			*(unsigned char *)POINTER(data) = (unsigned char)*((char *)value); break;
		case 'b':
			*(char *)POINTER(data) = (char)*(double *)value; break;
		case 'B':
			*(unsigned char *)POINTER(data) = (unsigned char)*(double *)value; break;
		case 's':
			*(short *)POINTER(data) = (short)*(double *)value; break;
		case 'S':
			*(unsigned short *)POINTER(data) = (unsigned short)*(double *)value; break;
		case 'i':
			*(int *)POINTER(data) = (int)*(double *)value; break;
		case 'I':
			*(unsigned int *)POINTER(data) = (unsigned int)*(double *)value; break;
		case 'l':
			*(long *)POINTER(data) = (long)*(double *)value; break;
		case 'L':
			*(unsigned long *)POINTER(data) = (unsigned long)*(double *)value; break;
		case 'f':
			*(float *)POINTER(data) = *(float *)value; break;
		case 'd':
			*(double *)POINTER(data) = *(double *)value;
			break;
		case '*':
			cp = *(char **)value;
			*(char **)POINTER(data) = malloc(strlen(cp) + 1);
			data->needToFreeStr = 1;
			strcpy(*(char **)POINTER(data), cp);
			break;

		case '^':
			*(((IoCFFIPointerData *)(IoObject_dataPointer(self)))->valuePointer) = *(void**)value;
			break;

		case '{':
		case '(':
			//TODO take this out of here
			memcpy(((IoCFFIStructureData *)(IoObject_dataPointer(self)))->buffer, value, ((IoCFFIStructureData *)(IoObject_dataPointer(self)))->ffiType.size);
			break;

		case '&':
			*(((IoCFFIFunctionData *)(IoObject_dataPointer(self)))->valuePointer) = *(void**)value;
			break;

		case '[':
			//TODO take this out of here
			memcpy(((IoCFFIArrayData *)(IoObject_dataPointer(self)))->buffer, value, ((IoCFFIArrayData *)(IoObject_dataPointer(self)))->ffiType.size);
			printf("IoCFFIDataType_setValueFromData ARRAY\n");
			break;

		case 'v':
			IoState_error_(IOSTATE, NULL, "attempt to setValue on void DataType");
			return IONIL(self);

		default:
			IoState_error_(IOSTATE, NULL, "unknown character '%c' in typeString", c);
			return IONIL(self);
	}

	return self;
}

IoObject *IoCFFIDataType_rawSetValue(IoCFFIDataType *self, IoObject *value)
{
	void *data;
	data = IoCFFIDataType_ValuePointerFromObject_(self, value);
	return IoCFFIDataType_setValueFromData(self, data);
}

IoCFFIDataType *IoCFFIDataType_setValue(IoCFFIDataType *self, IoObject *locals, IoMessage *m)
{
	return IoCFFIDataType_rawSetValue(self, IoMessage_locals_valueArgAt_(m, locals, 0));
}

IoObject *IoCFFIDataType_size(IoCFFIDataType *self, IoObject *locals, IoMessage *m)
{
	return IONUMBER(IoCFFIDataType_ffiType(self)->size);
}

IoObject *IoCFFIDataType_asBuffer(IoCFFIDataType *self, IoObject *locals, IoMessage *m)
{
	char *typeString, c;
	int len = 0, optLen = 0;
	unsigned char *buffer = NULL;

	if ( IoMessage_argCount(m) > 0 ) {
		optLen = IoMessage_locals_intArgAt_(m, locals, 0);
	}

	typeString = CSTRING( IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_objectFromData_") );
	switch ( c = typeString[0] ) {
		case 'c':
		case 'C':
		case 'b':
		case 'B':
		case 's':
		case 'S':
		case 'i':
		case 'I':
		case 'l':
		case 'L':
		case 'f':
		case 'd':
			len = IoCFFIDataType_ffiType(self)->size;
			buffer = (unsigned char *)POINTER(DATA(self));
			break;

		case 'v':
			break;

		case '*':
			if (*(char **)POINTER(DATA(self))) {
				len = strlen(*(char **)POINTER(DATA(self)));
				buffer = *(unsigned char **)POINTER(DATA(self));
			}
			break;

		//case '&':
		case '^':
			if(0 == optLen) {
				IoState_error_(IOSTATE, m, "Must specify length for Pointer types");
				return IONIL(self);
			}
			len = optLen;
			// We get the address where the pointer is stored, so we have to dereference once
			buffer = (unsigned char *)*((void**)IoCFFIPointer_valuePointer(self));
			break;

		case '{':
		case '(':
			len = IoCFFIStructure_ffiType(self)->size;
			buffer = (unsigned char *)IoCFFIStructure_valuePointer(self);
			break;

		case '[':
			len = IoCFFIArray_ffiType(self)->size;
			buffer = (unsigned char *)IoCFFIArray_valuePointer(self);
			break;

		default:
			IoState_error_(IOSTATE, m, "unknown character '%c' in typeString", c);
			return IONIL(self);
	}

	if ( buffer ) {
		if ( optLen && optLen < len ) len = optLen;
		return IoSeq_newWithData_length_(IOSTATE, buffer, len);
	}
	else
		return IONIL(self);
}


IoObject *IoCFFIDataType_objectFromData_(IoCFFIDataType *self, void *data)
{
	char *typeString, c;

	typeString = CSTRING( IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_objectFromData_") );
	switch ( c = typeString[0] ) {
		case 'c':
		case 'C':
			// see comment in IoCFFIDataType_ValuePointerFromObject_()
			return IoSeq_newWithCString_length_(IOSTATE, (char *)data, 1);
		case 'b':
			return IONUMBER(((double)(*((char *)data))));
		case 'B':
			return IONUMBER(((double)(*((unsigned char *)data))));
		case 's':
			return IONUMBER(((double)(*((short *)data))));
		case 'S':
			return IONUMBER(((double)(*((unsigned short *)data))));
		case 'i':
			return IONUMBER(((double)(*((int *)data))));
		case 'I':
			return IONUMBER(((double)(*((unsigned int *)data))));
		case 'l':
			return IONUMBER(((double)(*((long *)data))));
		case 'L':
			return IONUMBER(((double)(*((unsigned long *)data))));
		case 'f':
			return IONUMBER(((double)(*((float *)data))));
		case 'd':
			return IONUMBER((*((double *)data)));
		case 'v':
			return IONIL(self);

		case '*':
			if (*(char **)data) {
				return IoSeq_newWithCString_(IOSTATE, *(char **)data);
			}
			else {
				return IoSeq_new(IOSTATE);
			}

		case '^':
			return IoCFFIPointer_cloneWithData(self, (void **)data);

		case '{':
		case '(':
			return IoCFFIStructure_cloneWithData(self, data);

		case '&':
			return IoCFFIFunction_cloneWithData(self, (void **)data);

		case '[':
			return IoCFFIArray_cloneWithData(self, data);

		default:
			IoState_error_(IOSTATE, NULL, "unknown character '%c' in typeString", c);
			return IONIL(self);
	}
}


ffi_type *IoCFFIDataType_ffiType(IoCFFIDataType *self)
{
	char *typeString, c;

	typeString = CSTRING( IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_ffiType") );

	if ( strlen(typeString) < 1 ) {
		return NULL;
	}

	switch ( c = typeString[0] ) {
		case 'c':	return &ffi_type_schar;
		case 'C':	return &ffi_type_uchar;
		case 'b':	return &ffi_type_schar;
		case 'B':	return &ffi_type_uchar;
		case 's':	return &ffi_type_sshort;
		case 'S':	return &ffi_type_ushort;
		case 'i':	return &ffi_type_sint;
		case 'I':	return &ffi_type_uint;
		case 'l':	return &ffi_type_slong;
		case 'L':	return &ffi_type_ulong;
		case 'f':	return &ffi_type_float;
		case 'd':	return &ffi_type_double;
		case 'v':	return &ffi_type_void;

		case '*':
		case '^':
		case '&':	return &ffi_type_pointer;

		case '{':
		case '(':	return IoCFFIStructure_ffiType(self);

		case '[':	return IoCFFIArray_ffiType(self);

		default:
			IoState_error_(IOSTATE, NULL, "unknown character '%c' in typeString", c);
			return NULL;
	}
}

void *IoCFFIDataType_valuePointer(IoCFFIDataType* self)
{
	char c, *typeString;
	IoCFFIDataTypeData *data;

	typeString = CSTRING( IoState_on_doCString_withLabel_(IOSTATE, self, "typeString", "IoCFFIDataType_valuePointer") );
	data = DATA(self);

	switch ( c = typeString[0] ) {
		case 'c':
		case 'C':
		case 'b':
		case 'B':
		case 's':
		case 'S':
		case 'i':
		case 'I':
		case 'l':
		case 'L':
		case 'f':
		case 'd':
		case '*':
			return POINTER(data);

		case 'v':
			IoState_error_(IOSTATE, NULL, "atempt to get data pointer from Void type");
			return NULL;

		default:
			IoState_error_(IOSTATE, NULL, "unknown character '%c' in typeString", c);
			return NULL;
	}
}

void IoCFFIDataType_setValuePointer_(IoCFFIDataType* self, void *ptr)
{
	int offset = CNUMBER( IoObject_getSlot_(self, IOSYMBOL("_offset")) );

	if ( ISCFFIDataType(self) )          POINTER(DATA(self)) = ptr + offset;
	else if ( ISCFFIPointer(self) )      IoCFFIPointer_setValuePointer_offset_(self, ptr, offset);
	else if ( ISCFFIStructure(self) )    IoCFFIStructure_setValuePointer_offset_(self, ptr, offset);
	else if ( ISCFFIFunction(self) )     IoCFFIFunction_setValuePointer_offset_(self, ptr, offset);
	else if ( ISCFFIArray(self) )        IoCFFIArray_setValuePointer_offset_(self, ptr, offset);
}

