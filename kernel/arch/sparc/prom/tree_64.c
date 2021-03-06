

#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>

#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/ldc.h>

inline int __prom_getchild(int node)
{
	return p1275_cmd ("child", P1275_INOUT(1, 1), node);
}

inline int prom_getchild(int node)
{
	int cnode;

	if(node == -1) return 0;
	cnode = __prom_getchild(node);
	if(cnode == -1) return 0;
	return (int)cnode;
}
EXPORT_SYMBOL(prom_getchild);

inline int prom_getparent(int node)
{
	int cnode;

	if(node == -1) return 0;
	cnode = p1275_cmd ("parent", P1275_INOUT(1, 1), node);
	if(cnode == -1) return 0;
	return (int)cnode;
}

inline int __prom_getsibling(int node)
{
	return p1275_cmd(prom_peer_name, P1275_INOUT(1, 1), node);
}

inline int prom_getsibling(int node)
{
	int sibnode;

	if (node == -1)
		return 0;
	sibnode = __prom_getsibling(node);
	if (sibnode == -1)
		return 0;

	return sibnode;
}
EXPORT_SYMBOL(prom_getsibling);

inline int prom_getproplen(int node, const char *prop)
{
	if((!node) || (!prop)) return -1;
	return p1275_cmd ("getproplen", 
			  P1275_ARG(1,P1275_ARG_IN_STRING)|
			  P1275_INOUT(2, 1), 
			  node, prop);
}
EXPORT_SYMBOL(prom_getproplen);

inline int prom_getproperty(int node, const char *prop,
			    char *buffer, int bufsize)
{
	int plen;

	plen = prom_getproplen(node, prop);
	if ((plen > bufsize) || (plen == 0) || (plen == -1)) {
		return -1;
	} else {
		/* Ok, things seem all right. */
		return p1275_cmd(prom_getprop_name, 
				 P1275_ARG(1,P1275_ARG_IN_STRING)|
				 P1275_ARG(2,P1275_ARG_OUT_BUF)|
				 P1275_INOUT(4, 1), 
				 node, prop, buffer, P1275_SIZE(plen));
	}
}
EXPORT_SYMBOL(prom_getproperty);

inline int prom_getint(int node, const char *prop)
{
	int intprop;

	if(prom_getproperty(node, prop, (char *) &intprop, sizeof(int)) != -1)
		return intprop;

	return -1;
}
EXPORT_SYMBOL(prom_getint);


int prom_getintdefault(int node, const char *property, int deflt)
{
	int retval;

	retval = prom_getint(node, property);
	if(retval == -1) return deflt;

	return retval;
}
EXPORT_SYMBOL(prom_getintdefault);

/* Acquire a boolean property, 1=TRUE 0=FALSE. */
int prom_getbool(int node, const char *prop)
{
	int retval;

	retval = prom_getproplen(node, prop);
	if(retval == -1) return 0;
	return 1;
}
EXPORT_SYMBOL(prom_getbool);

void prom_getstring(int node, const char *prop, char *user_buf, int ubuf_size)
{
	int len;

	len = prom_getproperty(node, prop, user_buf, ubuf_size);
	if(len != -1) return;
	user_buf[0] = 0;
	return;
}
EXPORT_SYMBOL(prom_getstring);

int prom_nodematch(int node, const char *name)
{
	char namebuf[128];
	prom_getproperty(node, "name", namebuf, sizeof(namebuf));
	if(strcmp(namebuf, name) == 0) return 1;
	return 0;
}

int prom_searchsiblings(int node_start, const char *nodename)
{

	int thisnode, error;
	char promlib_buf[128];

	for(thisnode = node_start; thisnode;
	    thisnode=prom_getsibling(thisnode)) {
		error = prom_getproperty(thisnode, "name", promlib_buf,
					 sizeof(promlib_buf));
		/* Should this ever happen? */
		if(error == -1) continue;
		if(strcmp(nodename, promlib_buf)==0) return thisnode;
	}

	return 0;
}
EXPORT_SYMBOL(prom_searchsiblings);

inline char *prom_firstprop(int node, char *buffer)
{
	*buffer = 0;
	if(node == -1) return buffer;
	p1275_cmd ("nextprop", P1275_ARG(2,P1275_ARG_OUT_32B)|
			       P1275_INOUT(3, 0), 
			       node, (char *) 0x0, buffer);
	return buffer;
}
EXPORT_SYMBOL(prom_firstprop);

inline char *prom_nextprop(int node, const char *oprop, char *buffer)
{
	char buf[32];

	if(node == -1) {
		*buffer = 0;
		return buffer;
	}
	if (oprop == buffer) {
		strcpy (buf, oprop);
		oprop = buf;
	}
	p1275_cmd ("nextprop", P1275_ARG(1,P1275_ARG_IN_STRING)|
				    P1275_ARG(2,P1275_ARG_OUT_32B)|
				    P1275_INOUT(3, 0), 
				    node, oprop, buffer); 
	return buffer;
}
EXPORT_SYMBOL(prom_nextprop);

int
prom_finddevice(const char *name)
{
	if (!name)
		return 0;
	return p1275_cmd(prom_finddev_name,
			 P1275_ARG(0,P1275_ARG_IN_STRING)|
			 P1275_INOUT(1, 1), 
			 name);
}
EXPORT_SYMBOL(prom_finddevice);

int prom_node_has_property(int node, const char *prop)
{
	char buf [32];
        
	*buf = 0;
	do {
		prom_nextprop(node, buf, buf);
		if(!strcmp(buf, prop))
			return 1;
	} while (*buf);
	return 0;
}
EXPORT_SYMBOL(prom_node_has_property);

int
prom_setprop(int node, const char *pname, char *value, int size)
{
	if (size == 0)
		return 0;
	if ((pname == 0) || (value == 0))
		return 0;
	
#ifdef CONFIG_SUN_LDOMS
	if (ldom_domaining_enabled) {
		ldom_set_var(pname, value);
		return 0;
	}
#endif
	return p1275_cmd ("setprop", P1275_ARG(1,P1275_ARG_IN_STRING)|
					  P1275_ARG(2,P1275_ARG_IN_BUF)|
					  P1275_INOUT(4, 1), 
					  node, pname, value, P1275_SIZE(size));
}
EXPORT_SYMBOL(prom_setprop);

inline int prom_inst2pkg(int inst)
{
	int node;
	
	node = p1275_cmd ("instance-to-package", P1275_INOUT(1, 1), inst);
	if (node == -1) return 0;
	return node;
}

int
prom_pathtoinode(const char *path)
{
	int node, inst;

	inst = prom_devopen (path);
	if (inst == 0) return 0;
	node = prom_inst2pkg (inst);
	prom_devclose (inst);
	if (node == -1) return 0;
	return node;
}

int prom_ihandle2path(int handle, char *buffer, int bufsize)
{
	return p1275_cmd("instance-to-path",
			 P1275_ARG(1,P1275_ARG_OUT_BUF)|
			 P1275_INOUT(3, 1),
			 handle, buffer, P1275_SIZE(bufsize));
}
