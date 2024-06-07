/*
 * Copyright (c) 2024 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */
#ifndef ZRTOS_VFS_FILE_H
#define ZRTOS_VFS_FILE_H
#ifdef __cplusplus
extern "C" {
#endif


#include "zrtos_error.h"
#include "zrtos_bitfield.h"
#include "zrtos_vfs_dentry.h"


typedef struct _zrtos_vfs_file_t{
	zrtos_vfs_dentry_t *dentry;
	zrtos_vfs_offset_t offset;
}zrtos_vfs_file_t;

#ifndef ZRTOS_VFS_FILE_DESCRIPTOR__CFG_MAX
#error "define ZRTOS_VFS_FILE_DESCRIPTOR__CFG_MAX"
#endif

zrtos_vfs_file_t zrtos_vfs_file_index[ZRTOS_VFS_FILE_DESCRIPTOR__CFG_MAX];

zrtos_error_t zrtos_vfs_file__open(char *path,size_t *out){
	zrtos_error_t ret;
	zrtos_vfs_dentry_t *dentry = zrtos_vfs_dentry__lookup(
		 0
		,path
	);
	if(dentry){ 
		for(size_t fd=0;fd<ZRTOS_VFS_FILE_DESCRIPTOR__CFG_MAX;fd++){
			if(0 == zrtos_vfs_file_index[fd].dentry){
				zrtos_vfs_file_t *file = &zrtos_vfs_file_index[fd];

				dentry->count++;
				file->dentry = dentry;

				zrtos_vfs_plugin_t *plugin = file->dentry->inode.plugin;
				ret = ZRTOS_VFS_PLUGIN__INVOKE(
					 plugin
					,ZRTOS_VFS_PLUGIN_OPERATION__OPEN
					,file
				);
				if(ret == EXIT_SUCCESS){
					*out = fd;
				}else{
					dentry->count--;
					file->dentry = 0;
				}
				goto L_OUT;
			}
		}
		ret = EMFILE;
	}else{
		ret = ENOENT;
	}

L_OUT:
	return ret;
}

zrtos_error_t zrtos_vfs_file__close(size_t fd){
	zrtos_error_t ret;
	zrtos_vfs_file_t *file = &zrtos_vfs_file_index[fd];

	file->dentry->count--;
	ret = ZRTOS_VFS_PLUGIN__INVOKE(
		 file->dentry->inode.plugin
		,ZRTOS_VFS_PLUGIN_OPERATION__CLOSE
		,&file
	);
	file->dentry = 0;
	file->offset = 0;

	return ret;
}

zrtos_error_t zrtos_vfs_file__read(size_t fd,char *path,void *buffer,size_t len,size_t offset,size_t *ret){
	zrtos_vfs_file_t *file = &zrtos_vfs_file_index[fd];
	return file->dentry ? ZRTOS_VFS_PLUGIN__INVOKE(
		 file->dentry->inode.plugin
		,ZRTOS_VFS_PLUGIN_OPERATION__READ
		,file
		,path
		,buffer
		,len
		,offset
		,ret
	) : EBADF;
}

zrtos_error_t zrtos_vfs_file__write(size_t fd,char *path,void *buffer,size_t len,size_t offset,size_t *ret){
	zrtos_vfs_file_t *file = &zrtos_vfs_file_index[fd];
	return file->dentry ? ZRTOS_VFS_PLUGIN__INVOKE(
		 file->dentry->inode.plugin
		,ZRTOS_VFS_PLUGIN_OPERATION__WRITE
		,file
		,path
		,buffer
		,len
		,offset
		,ret
	) : EBADF;
}

zrtos_error_t zrtos_vfs_file__ioctl(size_t fd,char *path,int request,...){
	zrtos_error_t ret;
	zrtos_vfs_file_t *file = &zrtos_vfs_file_index[fd];
	va_list       args;

	va_start(args,request);
	ret = file->dentry ? ZRTOS_VFS_PLUGIN__INVOKE(
		 file->dentry->inode.plugin
		,ZRTOS_VFS_PLUGIN_OPERATION__IOCTL
		,&file
		,path
		,request
		,args
	) : EBADF;
	va_end(args);

	return ret;
}

void zrtos_vfs_file__set_inode_data(zrtos_vfs_file_t *thiz,void *ctx){
	thiz->dentry->inode.ctx = ctx;
}

void *zrtos_vfs_file__get_inode_data(zrtos_vfs_file_t *thiz){
	return thiz->dentry->inode.ctx;
}

void zrtos_vfs_file__set_offset(zrtos_vfs_file_t *thiz,zrtos_vfs_offset_t offset){
	thiz->offset = offset;
}

zrtos_vfs_offset_t zrtos_vfs_file__get_offset(zrtos_vfs_file_t *thiz){
	return thiz->offset;
}

#ifdef __cplusplus
}
#endif
#endif