/****************************************************************************
 * Copyright (c) 2012, the Konoha project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/* ************************************************************************ */

#include "konoha_lkm.h"
#include "konoha2/konoha2.h"
#include "konoha2/klib.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("uchida atsushi");

enum {
	MAXCOPYBUF = 256
};

struct konohadev_t {
	dev_t id;
	struct cdev cdev;
	konoha_t  konoha;
	char* buffer;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))
	struct semaphore sem;
#endif
};

static const char *msg = "konohadev";
static struct konohadev_t *konohadev_p;

static int knh_dev_open (struct inode *inode , struct file *filp);
static ssize_t knh_dev_read(struct file *filp, char __user *user_buf,
		size_t count, loff_t *offset);

static ssize_t knh_dev_write(struct file *filp,const char __user *user_buf,
		size_t count,loff_t *offset) ;

static struct file_operations knh_fops = {
	.owner = THIS_MODULE,
	.open  = knh_dev_open,
	.read  = knh_dev_read,
	.write = knh_dev_write,
};

static int knh_dev_open (struct inode* inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev,
			struct konohadev_t,cdev);
	return 0;
}

static ssize_t knh_dev_read (struct file* filp, char __user *user_buf,
		size_t count, loff_t *offset)
{
	struct konohadev_t *dev = filp->private_data;
	char buf[MAXCOPYBUF];
	int len;

	if(*offset > 0) return 0;

	if(down_interruptible(&dev->sem)){
		return -ERESTARTSYS;
	}

	//printk(KERN_ALERT "read\n");
	len = snprintf(buf,MAXCOPYBUF,"%s\n",dev->buffer);

	if(copy_to_user(user_buf,buf,len)){
		up(&dev->sem);
		printk(KERN_ALERT "%s: copy_to_user failed\n",msg);
		return -EFAULT;
	}

	up(&dev->sem);
	*offset += len;

	return len;
}

static const char* kbegin(kinfotag_t t)
{
	return "";
}

static const char* kend(kinfotag_t t)
{
	return "";
}

#define LKM_BUFFER_SIZE 256
static int kvprintf_i(const char *fmt, va_list args)
{
	char buffer[LKM_BUFFER_SIZE] = {0};
	vsnprintf(buffer,LKM_BUFFER_SIZE, fmt, args);
	strncat(konohadev_p->buffer,buffer,LKM_BUFFER_SIZE);
	return 0;
}

static int printf_(const char *fmt, ...)
{
	char buffer[LKM_BUFFER_SIZE] = {0};
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer,LKM_BUFFER_SIZE, fmt, ap);
	va_end(ap);
	strncat(konohadev_p->buffer,buffer,LKM_BUFFER_SIZE);
	return 0;
}

const kplatform_t* platform_kernel(void)
{
	static kplatform_t plat = {
		.name = "lkm",
		.stacksize = K_PAGESIZE * 4,
		.malloc_i    = malloc,
		.free_i      = free,
		.setjmp_i    = setjmp,
		.longjmp_i   = longjmp,
		.realpath_i  = realpath,
		.fopen_i     = fopen,
		.fgetc_i     = fgetc,
		.feof_i      = feof,
		.fclose_i    = fclose,
		.syslog_i    = syslog,
		.vsyslog_i   = NULL,
		.printf_i    = printf_,
		.vprintf_i   = kvprintf_i,
		.snprintf_i  = snprintf,
		.vsnprintf_i = vsnprintf,
		.qsort_i     = qsort,
		.exit_i      = kexit,
		.packagepath = NULL,
		.exportpath  = NULL,
		.begin       = kbegin,
		.end         = kend,
		.dbg_p       = kdbg_p
	};
	return (const kplatform_t*)(&plat);
}

static void CTX_evalScript(CTX, char *data, size_t len)
{
	kwb_t wb;
	kwb_init(&(_ctx->stack->cwb), &wb);
	kwb_write(&wb,data,len);
	kline_t uline = FILEID_("(kernel)") | 1;
	konoha_eval((konoha_t)_ctx, kwb_top(&wb,1),uline);
	kwb_free(&wb);
}
//EXPORT_SYMBOL(CTX_evalScript);

static ssize_t knh_dev_write(struct file *filp,const char __user *user_buf,
		size_t count,loff_t *offset) {
	char buf[MAXCOPYBUF];
	struct konohadev_t *dev = filp->private_data;
	long len;
	if(down_interruptible(&dev->sem)){
		return -ERESTARTSYS;
	}
	len = copy_from_user(buf,user_buf,count);
	memset(dev->buffer,0,sizeof(char)*MAXCOPYBUF);
	buf[count] = '\0';
	CTX_evalScript((CTX_t)dev->konoha,buf,strlen(buf));
//	snprintf(dev->buffer,MAXCOPYBUF,"%s",((CTX_t)dev->konoha)->buffer);
//	strncpy(((CTX_t)dev->konoha)->buffer,"\0",1);
//	printk(KERN_DEBUG "[%s][dev->buffer='%s']\n",__func__ ,dev->buffer);
	up(&dev->sem);
	return count;
}


static void knh_dev_setup(struct konohadev_t *dev)
{
	int err = alloc_chrdev_region(&dev->id, 0, 1, msg);
	if(err){
		printk(KERN_ALERT "%s: alloc_chrdev_region() failed (%d)\n",
				msg,err);
		return;
	}
	cdev_init(&dev->cdev,&knh_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->konoha = konoha_open(platform_kernel());
	dev->buffer = kmalloc(sizeof(char)*MAXCOPYBUF,GFP_KERNEL);
	memset(dev->buffer,0,sizeof(char)*MAXCOPYBUF);
	sema_init(&dev->sem,1);

	err = cdev_add(&dev->cdev, dev->id, 1);
	if (err) {
		printk(KERN_ALERT "%s:cdev_add() failed(%d)\n",msg,err);
	}
}

// Start/Init function
static int __init konohadev_init(void) {
	konohadev_p = kmalloc(sizeof(struct konohadev_t),GFP_KERNEL);
	if(!konohadev_p){
		printk(KERN_ALERT "%s:kmalloc() failed\n",msg);
		return -ENOMEM;
	}
	memset(konohadev_p,0,sizeof(struct konohadev_t));
	printk(KERN_INFO "konoha init!\n");
	knh_dev_setup(konohadev_p);
	return 0;
}

// End/Cleanup function
static void __exit konohadev_exit(void) {
	konoha_close(konohadev_p->konoha);
	kfree(konohadev_p->buffer);
	cdev_del(&konohadev_p->cdev);
	unregister_chrdev_region(konohadev_p->id,1);
	printk(KERN_INFO "konoha exit\n");
	kfree(konohadev_p);
}

module_init(konohadev_init);
module_exit(konohadev_exit);
