/*
 * Copyright (c) 2024 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */
#ifndef ZRTOS_MEM_H
#define ZRTOS_MEM_H
#ifdef __cplusplus
extern "C" {
#endif


#include "zrtos_debug.h"
#include "zrtos_mem_chunk.h"


#define ZRTOS_MEM__INIT(name,heap_size)                 \
  static struct{                                        \
    unsigned char heap[heap_size];                      \
  }__attribute__((aligned(ZRTOS__BYTE_ALIGNMENT)))name;


#define ZRTOS_MEM__GET(name) \
  &(name.heap[0])

typedef struct _zrtos_mem_t{
	void                    *ptr;
	size_t                  length;//index length
	size_t                  heap_size;
	size_t                  total_size;
}zrtos_mem_t;

bool zrtos_mem__init(
	 zrtos_mem_t *thiz
	,void        *heap
	,size_t      heap_size
){
	ZRTOS__DEBUG_CODE(
		zrtos_debug__memset(heap,0xEE,heap_size);
	);

	thiz->ptr = heap;
	thiz->length = 0;
	thiz->heap_size = 0;
	thiz->total_size = heap_size;

	return true;
}
/*
void _zrtos_mem__swap_uint8(uint8_t *src,uint8_t *dest){
	uint8_t tmp = *src;
	*src = *dest;
	*dest = tmp;
}
*/
void _zrtos_mem__swap_to_end(
	 void *buffer
	,size_t length
	,size_t buffer_length
){
	size_t l = buffer_length - length;
	uint8_t *src = buffer;
	uint8_t *dest = src + length;
	while(l--){
		ZRTOS_TYPES__SWAP_PTR_CONTENTS(src,dest);
		src++;
		dest++;
		//_zrtos_mem__swap_uint8(src++,dest++);
	}
}

void *zrtos_mem__get_last_address(zrtos_mem_t *thiz){
	return zrtos_types__ptr_add(thiz->ptr,thiz->total_size);
}

void *zrtos_mem__get_stack_ptr(zrtos_mem_t *thiz){
	return ZRTOS__GET_STACK_PTR();
}

size_t _zrtos_mem__get_free_space(zrtos_mem_t *thiz){
	return
		  thiz->ptr < zrtos_mem__get_stack_ptr(thiz)
		? zrtos_types__ptr_get_byte_distance(
			ZRTOS_TYPES__MIN(
				 zrtos_mem__get_stack_ptr(thiz)
				,zrtos_mem__get_last_address(thiz)
			)
			,thiz->ptr
		)
		: 0
	;
}

zrtos_mem_chunk_t *zrtos_mem__get_by_id(
	 zrtos_mem_t *thiz
	,zrtos_mem_chunk_uid_t   uid
){
	zrtos_mem_chunk_t *node = thiz->ptr;
	for(size_t l = thiz->length;l--;node++){
		if(node->uid.uid == uid.uid){
			return node;
		}
	}
	return 0;
}

zrtos_mem_chunk_uid_t zrtos_mem__get_next_uid(
	zrtos_mem_t *thiz
){
	static zrtos_mem_chunk_uid_t uid = {
		.uid = 1
	};
	zrtos_mem_chunk_uid_t ret = {
		.uid = uid.uid++
	};

	while(ret.uid == 0 || zrtos_mem__get_by_id(
		 thiz
		,ret
	)){
		ret.uid++;
	}

	return ret;
}

#ifdef ZRTOS__USE_MEMMOVE
# define _zrtos_mem__memmove_right_overlapping memmove
# define _zrtos_mem__memmove_left_overlapping memmove
#else
void _zrtos_mem__memmove_right_overlapping(
	 uint8_t *dest
	,uint8_t *src
	,size_t length
){
	dest += length;
	src += length;
	while(length--){
		*dest-- = *src--;
	}
}

void zrtos_mem__memmove_left_overlapping(
	  uint8_t *dest
	 ,uint8_t *src
	 ,size_t length
){
	while(length--){
		*dest++ = *src++;
	}
}
#endif

zrtos_mem_chunk_uid_t zrtos_mem__malloc(
	 zrtos_mem_t *thiz
	,zrtos_mem_type_t type
	,size_t length
){
	size_t free_space = _zrtos_mem__get_free_space(thiz);
	size_t length_total = sizeof(zrtos_mem_chunk_t) + length;
	if(free_space >= length_total){
		zrtos_mem_chunk_t *chunk;
		zrtos_mem_chunk_t *node;
		size_t index_length = (sizeof(zrtos_mem_chunk_t) * thiz->length);
		size_t heap_length = thiz->heap_size - index_length;

		uint8_t *src = thiz->ptr + thiz->heap_size;
		uint8_t *dest = src + sizeof(zrtos_mem_chunk_t);

		thiz->heap_size += length_total;

		_zrtos_mem__memmove_right_overlapping(
			 dest - heap_length
			,src - heap_length
			,heap_length
		);

		chunk = &((zrtos_mem_chunk_t*)thiz->ptr)[thiz->length++];
		chunk->ptr = thiz->ptr + thiz->heap_size - length;
		chunk->length = length;
		chunk->uid = zrtos_mem__get_next_uid(thiz);
		chunk->type.type = type;

		//update index
		node = &((zrtos_mem_chunk_t*)thiz->ptr)[0];
		while(++node < chunk){
			node->ptr += sizeof(zrtos_mem_chunk_t);
		}

		ZRTOS__DEBUG_CODE(
			static uint8_t pattern = 0xE0;
			zrtos_debug__memset(chunk->ptr,(int)(pattern++),chunk->length);
		);

		return chunk->uid;
	}
	return zrtos_mem_chunk_uid__error();
}

void _zrtos_mem__free(zrtos_mem_t *thiz,zrtos_mem_chunk_t *chunk){
	uint8_t *dest = (uint8_t*)chunk;
	uint8_t *src = dest + sizeof(zrtos_mem_chunk_t);
	zrtos_mem_chunk_t *node;
	zrtos_mem_chunk_t *sentinel;
	size_t length = chunk->length;
	size_t length_total = sizeof(zrtos_mem_chunk_t) + length;
	int lp = 1;

	//update index
	node = chunk;
	sentinel = &((zrtos_mem_chunk_t*)thiz->ptr)[thiz->length];
	while(++node < sentinel){
		node->ptr -= length;
	}

	//1st loop delete index, 2nd loop delete element
	sentinel = zrtos_types__ptr_add(chunk->ptr,sizeof(zrtos_mem_chunk_t));
	do{
		while(src < ((uint8_t*)sentinel)){
 			*dest++ = *src++;
		}
		src += length;
		sentinel = zrtos_types__ptr_add(thiz->ptr,thiz->heap_size);
	}while(lp--);

	thiz->length--;
	thiz->heap_size -= length_total;

	ZRTOS__DEBUG_CODE(
		zrtos_debug__memset(
			 zrtos_types__ptr_add(thiz->ptr,thiz->heap_size)
			,0xEE
			,length_total
		);
	);
}

void zrtos_mem__free(zrtos_mem_t *thiz,zrtos_mem_chunk_uid_t uid){
	zrtos_mem_chunk_t *chunk = zrtos_mem__get_by_id(
		 thiz
		,uid
	);
	_zrtos_mem__free(thiz,chunk);
}

void *_zrtos_mem__swap_to_heap_end(
	 void   *heap
	,size_t heap_length
	,size_t used_length
	,size_t chunk_offset
	,size_t chunk_length
){
	uint8_t *buffer = heap;
	void *ret = buffer + heap_length - chunk_length;
	_zrtos_mem__swap_to_end(
		 buffer + chunk_offset
		,chunk_length
		,used_length - chunk_offset
	);
	_zrtos_mem__memmove_right_overlapping(
		 ret-1//buffer + heap_length - chunk_length
		,buffer + used_length - chunk_length-1
		,chunk_length
	);
	return ret;
}

void *zrtos_mem__page_in(
	 zrtos_mem_t       *thiz
	,zrtos_mem_chunk_t *chunk
){
	size_t index_length = (sizeof(zrtos_mem_chunk_t) * thiz->length);
	void *ptr_index = zrtos_types__ptr_add(thiz->ptr,index_length);
	void *ret = _zrtos_mem__swap_to_heap_end(
		 ptr_index
		,thiz->total_size - index_length
		,thiz->heap_size - index_length
		,zrtos_types__ptr_get_byte_distance(chunk->ptr,ptr_index)
		,chunk->length
	);

	thiz->heap_size -= chunk->length;
	//chunk->ptr = 0;
	chunk->length = 0;
	chunk->type.type = ZRTOS_MEM_CHUNK_TYPE__TASK_ACTIVE;

	return ret;
}

void zrtos_mem__page_out(
	 zrtos_mem_t       *thiz
	,zrtos_mem_chunk_t *chunk
	,size_t            length
){
	uint8_t *heap_end_ptr = zrtos_types__ptr_add(thiz->ptr,thiz->heap_size);
	zrtos_mem__memmove_left_overlapping(
		 heap_end_ptr
		,zrtos_types__ptr_add(thiz->ptr,thiz->total_size - length)
		,length
	);

	thiz->heap_size += length;

	chunk->ptr = heap_end_ptr;
	chunk->length = length;
	chunk->type.type = ZRTOS_MEM_CHUNK_TYPE__TASK_IDLE;

	//move chunk to end of index
	size_t index_length = (sizeof(zrtos_mem_chunk_t) * thiz->length);
	_zrtos_mem__swap_to_end(
		 chunk
		,sizeof(zrtos_mem_chunk_t)
		,zrtos_types__ptr_get_byte_distance(
			zrtos_types__ptr_add(
				 thiz->ptr
				,index_length
			)
			,chunk
		)
	);
}

#define ZRTOS_MEM__EACH_BEGIN(thiz,type,value)                                 \
	for(size_t l__ = 0,len__=(thiz)->length;l__ < len__;l__++){                \
		zrtos_mem_chunk_t *value = &(((zrtos_mem_chunk_t*)(thiz))->ptr)[0];    \
		if(zrtos_mem_chunk_type__is_eq(zrtos_mem_chunk__get_type(value),type))

#define ZRTOS_MEM__EACH_END \
		}                   \
	}


#ifdef __cplusplus
}
#endif
#endif