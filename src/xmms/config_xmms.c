#include <glib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdlib.h>
#include <string.h>

#include "config_xmms.h"

/* Huge configlock */
static GMutex *config_lock;

#define XMMS_CONFIG_LOCK g_mutex_lock (config_lock)
#define XMMS_CONFIG_UNLOCK g_mutex_unlock (config_lock)

/*
 * The entire structure of this configfile and internal structures
 * orginated from tilde (http://tilde.sourceforge.net) but has
 * been redesigned to fit XMMS. 
 *
 * As now XMMS is not able to use "lists" as config values.
 */


/*
 * Public functions
 */


/*
 * sets the associated data to a xmms_config_value_t
 */

void 
xmms_config_value_data_set (xmms_config_value_t *value, gchar *data) 
{
	if(!value || !data) return;

	XMMS_CONFIG_LOCK;
	value -> data = data;
	XMMS_CONFIG_UNLOCK;
}

/*
 * adds a XML child to 'val' with 'name' as name and 'data' as associated
 * data. the directive name will be "property"
 *
 * this functions should be used by plugins for adding properties to its
 * own node in the XML config tree.
 */

gboolean
xmms_config_add_property (xmms_config_value_t *val, gchar *name, gchar *data) 
{
	xmms_config_value_t *new;

	new = xmms_config_value_create (XMMS_CONFIG_VALUE_PLAIN, "property");
	xmms_config_value_prop_add (new, "name", name);
	xmms_config_value_data_set (new, data);

	XMMS_CONFIG_LOCK;
	xmms_config_value_list_add (val, new);
	XMMS_CONFIG_UNLOCK;

	return TRUE;
}

/*
 * Looks up a value from the hashtable 
 */

xmms_config_value_t *
xmms_config_value_lookup (GHashTable *h, const gchar *valuename)
{
	xmms_config_value_t *ret;
	if (!h || !valuename) return NULL;

	XMMS_CONFIG_LOCK;
	ret = (xmms_config_value_t *) g_hash_table_lookup (h, valuename);
	XMMS_CONFIG_UNLOCK;

	return ret;
}

/*
 * This adds a section to the config file, this should be called
 * by a plugin the first time to create an own node in the XML
 * tree.
 */

xmms_config_value_t *
xmms_config_add_section (GHashTable *h, gchar *name)
{
	xmms_config_value_t *new;

	new = xmms_config_value_create (XMMS_CONFIG_VALUE_LIST, name);
	XMMS_CONFIG_LOCK;
	g_hash_table_insert (h, name, new);
	XMMS_CONFIG_UNLOCK;
	return new;
}

/*
 * Looks up a value with the name 'subvalue'
 */

xmms_config_value_t *
xmms_config_value_list_lookup (xmms_config_value_t *value, const gchar *subvalue) 
{
	xmms_config_value_t *tmp;

	if(value->type!=XMMS_CONFIG_VALUE_LIST || !value->child) return NULL;

	XMMS_CONFIG_LOCK;

	tmp = value->child;

	while(tmp) {
		if(g_strcasecmp(tmp->directive,subvalue)==0) {
			XMMS_CONFIG_UNLOCK;
			return tmp;
		}
		tmp=tmp->next;
	}

	XMMS_CONFIG_UNLOCK;

	return NULL;
}

/*
 * Looks up a value with the property name set to 'property'
 */

xmms_config_value_t *
xmms_config_value_property_lookup (xmms_config_value_t *val, const gchar *property)
{
	xmms_config_value_t *tmp;

	if (val->type!=XMMS_CONFIG_VALUE_LIST || !val->child) return NULL;

	XMMS_CONFIG_LOCK;
	tmp = val->child;

	while (tmp) {
		if (g_strcasecmp (xmms_config_value_getprop (tmp, "name"), property) == 0) {
			XMMS_CONFIG_UNLOCK;
			return tmp;
		}
		tmp=tmp->next;
	}
	
	XMMS_CONFIG_UNLOCK;
	return NULL;

}

/*
 * returns the type of 'value'
 */

gint xmms_config_value_type (xmms_config_value_t *value) {
	gint ret;

	ret = value->type;

	return ret;
}

/*
 * returns the data of 'value' as a int
 */

gint xmms_config_value_as_int (xmms_config_value_t *value) {
	gint i;

	if(!value || value->type != XMMS_CONFIG_VALUE_PLAIN || !value->data) return -1;

	XMMS_CONFIG_LOCK;
	i=strtol(value->data,NULL,10);
	XMMS_CONFIG_UNLOCK;

	return i;
}

/*
 * returns the data of 'value' as a string
 */

gchar *xmms_config_value_as_string (xmms_config_value_t *value) {
	gchar *ret;
	if(!value || value->type != XMMS_CONFIG_VALUE_PLAIN || !value->data) return NULL;

	XMMS_CONFIG_LOCK;
	ret = value->data;
	XMMS_CONFIG_UNLOCK;
	
	return ret;
}

/*
 * Private functions
 */

void 
xmms_config_add_prop (GHashTable *config, xmms_config_value_t *value, xmlNodePtr node);

void
xmms_config_save_section (gpointer key, gpointer data, gpointer udata) 
{
	FILE *fp = (FILE*)udata;
	xmms_config_value_t *val = (xmms_config_value_t*)data;
	gchar *sect;
	gint p=0;	


	if (xmms_config_value_type (val) == XMMS_CONFIG_VALUE_LIST) {
		sect = g_strdup_printf ("\t\t<%s>\n", (gchar *)key);
		fwrite (sect, strlen (sect), 1, fp);
		g_free (sect);
		p = 1;
		if (val->child)
			val=val->child;
	}

	while (val) {
		if (val->directive) {
			sect = g_strdup_printf ("\t\t\t<%s>%s</%s>\n", 
					val->directive,
					val->data ? val->data : "",
					val->directive);
			fwrite (sect, strlen (sect), 1, fp);
			g_free (sect);
		}

		val=val->next;
	}

	if (p) {
		sect = g_strdup_printf ("\t\t</%s>\n", (gchar *)key);
		fwrite (sect, strlen (sect), 1, fp);
		g_free (sect);
	}

}

gboolean
xmms_config_save_to_file (xmms_config_data_t *config, gchar *filename) 
{

	FILE *fp;

	fp = fopen (filename, "w+");

	if (!fp) return FALSE;
	
	XMMS_CONFIG_LOCK;

	/* write header */
	fwrite ("<XMMS>\n\n", 8, 1, fp);

	fwrite ("\t<core>\n",8, 1, fp);
	g_hash_table_foreach (config->core, xmms_config_save_section, (gpointer) fp);
	fwrite ("\t</core>\n",9, 1, fp);
	fwrite ("\t<decoder>\n",11, 1, fp);
	g_hash_table_foreach (config->decoder, xmms_config_save_section, (gpointer) fp);
	fwrite ("\t</decoder>\n",12, 1, fp);
	fwrite ("\t<output>\n",10, 1, fp);
	g_hash_table_foreach (config->output, xmms_config_save_section, (gpointer) fp);
	fwrite ("\t</output>\n",11, 1, fp);
	fwrite ("\t<transport>\n",13, 1, fp);
	g_hash_table_foreach (config->transport, xmms_config_save_section, (gpointer) fp);
	fwrite ("\t</transport>\n\n",14, 1, fp);

	fwrite ("</XMMS>\n", 8, 1, fp);
	XMMS_CONFIG_UNLOCK;

	return TRUE;
}

void 
xmms_config_clean (void *key, void *value, void *data)
{
	g_hash_table_destroy((GHashTable *)value);
}

void 
xmms_config_deinit (xmms_config_data_t *data) 
{
	/* g_hash_table_foreach_remove () */
}

xmms_config_data_t *
xmms_config_init (gchar *filename) 
{
	xmlDocPtr doc;
	xmlNodePtr decodenode=NULL,outputnode=NULL,corenode=NULL,root,transportnode=NULL;
	extern gint xmlDoValidityCheckingDefaultValue;
	GHashTable *decodertable;
	GHashTable *outputtable;
	GHashTable *coretable;
	GHashTable *transporttable;
	xmms_config_data_t *ret;
	xmlDoValidityCheckingDefaultValue=0;

	if(!(doc = xmlParseFile(filename))) {
		return NULL;
	}

	root = xmlDocGetRootElement(doc);
	root = root->children;

	config_lock = g_mutex_new ();

	decodertable = g_hash_table_new (g_str_hash, g_str_equal);
	outputtable = g_hash_table_new (g_str_hash, g_str_equal);
	transporttable = g_hash_table_new (g_str_hash, g_str_equal);
	coretable = g_hash_table_new (g_str_hash, g_str_equal);

	while(root) {
		if (g_strcasecmp ("core",(char*)root->name)==0) {
			corenode = root -> children;
		} else if (g_strcasecmp ("decoder",(char*)root->name)==0) {
			decodenode = root -> children;
		} else if (g_strcasecmp ("output",(char*)root->name)==0) {
			outputnode = root -> children;
		} else if (g_strcasecmp("transport",(char*)root->name)==0) {
			transportnode = root -> children;
		}
		root=root->next;
	}

	xmms_config_add_prop (coretable, NULL, corenode);
	xmms_config_add_prop (decodertable, NULL, decodenode);
	xmms_config_add_prop (transporttable, NULL, transportnode);
	xmms_config_add_prop (outputtable, NULL, outputnode);

	ret = g_new0 (xmms_config_data_t, 1);

	ret->core = coretable;
	ret->decoder = decodertable;
	ret->output = outputtable;
	ret->transport = transporttable;

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return ret;

}

void 
xmms_config_add_prop (GHashTable *config, xmms_config_value_t *value, xmlNodePtr node) 
{
	while(node) {
		if(node->type==XML_ELEMENT_NODE) {
			xmms_config_value_t *n;
			gint current;
			xmlAttrPtr xprop;
			
			/* primitive way to find out if this is a list or a plain value */
			if (node -> children && node -> children -> next) {
				current = XMMS_CONFIG_VALUE_LIST;
			} else {
				current = XMMS_CONFIG_VALUE_PLAIN;
			}

			n = xmms_config_value_create (current, g_strdup((char*)node->name));

			xprop = node -> properties;

			while (xprop) {
				xmms_config_value_prop_add (n, g_strdup((char*)xprop->name), g_strdup((char*)xprop->children->content));
				xprop=xprop->next;
			}

			if (xmms_config_value_type(n) == XMMS_CONFIG_VALUE_PLAIN && 
			    node -> children && node -> children -> content) {

				gchar *value;
				
				value = g_strdup ((char*)node->children->content);
				xmms_config_value_data_set(n,value);

			} else {
				xmms_config_add_prop(NULL,n,node->children);
			}

			if(value && !config) {
				xmms_config_value_list_add (value,n);
			} else {
				g_hash_table_insert (config,n->directive,n);
			}
				
		}
		node=node->next;
	}

}

xmms_config_value_t *
xmms_config_value_create (gint type, gchar *directive) 
{
	xmms_config_value_t *ret;

	if(!directive) return NULL;

	if(type == XMMS_CONFIG_VALUE_PLAIN || type == XMMS_CONFIG_VALUE_LIST) {
		ret = g_new0 (xmms_config_value_t, 1);
		ret->type=type;
		ret->directive=directive;
		ret->prop = NULL;
		return ret;
	}



	return NULL;
}

void 
xmms_config_value_list_add ( xmms_config_value_t *value, 
				  xmms_config_value_t *add) 
{
	xmms_config_value_t *tmp;
	
	if(value->type!=XMMS_CONFIG_VALUE_LIST) return;

	tmp=value->child;

	if(!tmp)  {
		value->child=add;
	} else {

		while(tmp->next) {
			tmp=tmp->next;
		}

		tmp->next=add;
	}

	return;
}

gchar *
xmms_config_value_getprop (xmms_config_value_t *value,char *propname) 
{
	if(!value->prop) return NULL;

	return g_hash_table_lookup (value->prop,propname);
}

void 
xmms_config_value_prop_add (xmms_config_value_t *value,
				  gchar *propname,
				  gchar *data) 
{
	if(!value->prop) {
		value->prop = g_hash_table_new (g_str_hash,g_str_equal);	
	}

	g_hash_table_insert (value->prop,(void*)propname,(void*)data);
}
	

