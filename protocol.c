#include "objc/runtime.h"
#include "protocol.h"
#include "properties.h"
#include "lock.h"
#include <stdlib.h>

#define BUFFER_TYPE struct objc_protocol_list
#include "buffer.h"

// Get the functions for string hashing
#include "string_hash.h"

static int protocol_compare(const char *name, 
                            const struct objc_protocol2 *protocol)
{
	return string_compare(name, protocol->name);
}
static int protocol_hash(const struct objc_protocol2 *protocol)
{
	return string_hash(protocol->name);
}
#define MAP_TABLE_NAME protocol
#define MAP_TABLE_COMPARE_FUNCTION protocol_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE protocol_hash
#include "hash_table.h"

static protocol_table *known_protocol_table;

void __objc_init_protocol_table(void)
{
	known_protocol_table = protocol_create(128);
}  

static void protocol_table_insert(const struct objc_protocol2 *protocol)
{
	protocol_insert(known_protocol_table, (void*)protocol);
}

struct objc_protocol2 *protocol_for_name(const char *name)
{
	return protocol_table_get(known_protocol_table, name);
}

static id ObjC2ProtocolClass = 0;

static int isEmptyProtocol(struct objc_protocol2 *aProto)
{
	int isEmpty = 
		((aProto->instance_methods == NULL) || 
			(aProto->instance_methods->count == 0)) &&
		((aProto->class_methods == NULL) || 
			(aProto->class_methods->count == 0)) &&
		((aProto->protocol_list == NULL) ||
		  (aProto->protocol_list->count == 0));
	if (aProto->isa == ObjC2ProtocolClass)
	{
		struct objc_protocol2 *p2 = (struct objc_protocol2*)aProto;
		isEmpty &= (p2->optional_instance_methods->count == 0);
		isEmpty &= (p2->optional_class_methods->count == 0);
		isEmpty &= (p2->properties->count == 0);
		isEmpty &= (p2->optional_properties->count == 0);
	}
	return isEmpty;
}

// FIXME: Make p1 adopt all of the stuff in p2
static void makeProtocolEqualToProtocol(struct objc_protocol2 *p1,
                                        struct objc_protocol2 *p2) 
{
#define COPY(x) p1->x = p2->x
	COPY(instance_methods);
	COPY(class_methods);
	COPY(protocol_list);
	if (p1->isa == ObjC2ProtocolClass &&
		p2->isa == ObjC2ProtocolClass)
	{
		COPY(optional_instance_methods);
		COPY(optional_class_methods);
		COPY(properties);
		COPY(optional_properties);
	}
#undef COPY
}

static struct objc_protocol2 *unique_protocol(struct objc_protocol2 *aProto)
{
	if (ObjC2ProtocolClass == 0)
	{
		ObjC2ProtocolClass = objc_getClass("Protocol2");
	}
	struct objc_protocol2 *oldProtocol = 
		protocol_for_name(aProto->name);
	if (NULL == oldProtocol)
	{
		// This is the first time we've seen this protocol, so add it to the
		// hash table and ignore it.
		protocol_table_insert(aProto);
		return aProto;
	}
	if (isEmptyProtocol(oldProtocol))
	{
		if (isEmptyProtocol(aProto))
		{
			return aProto;
			// Add protocol to a list somehow.
		}
		else
		{
			// This protocol is not empty, so we use its definitions
			makeProtocolEqualToProtocol(oldProtocol, aProto);
			return aProto;
		}
	}
	else
	{
		if (isEmptyProtocol(aProto))
		{
			makeProtocolEqualToProtocol(aProto, oldProtocol);
			return oldProtocol;
		}
		else
		{
			return oldProtocol;
			//FIXME: We should really perform a check here to make sure the
			//protocols are actually the same.
		}
	}
}

static id protocol_class;
static id protocol_class2;
enum protocol_version
{
	/**
	 * Legacy (GCC-compatible) protocol version.
	 */
	protocol_version_legacy = 2,
	/**
	 * New (Objective-C 2-compatible) protocol version.
	 */
	protocol_version_objc2 = 3
};

static BOOL init_protocols(struct objc_protocol_list *protocols)
{
	// Protocol2 is a subclass of Protocol, so if we have loaded Protocol2 we
	// must have also loaded Protocol.
	if (nil == protocol_class2)
	{
		protocol_class = objc_getClass("Protocol");
		protocol_class2 = objc_getClass("Protocol2");
	}
	if (nil == protocol_class2)
	{
		return NO;
	}

	for (unsigned i=0 ; i<protocols->count ; i++)
	{
		struct objc_protocol2 *aProto = protocols->list[i];
		// Don't initialise a protocol twice
		if (aProto->isa == protocol_class ||
			aProto->isa == protocol_class2) { continue ;}

		// Protocols in the protocol list have their class pointers set to the
		// version of the protocol class that they expect.
		enum protocol_version version = 
			(enum protocol_version)(uintptr_t)aProto->isa;
		switch (version)
		{
			default:
				fprintf(stderr, "Unknown protocol version");
				abort();
			case protocol_version_legacy:
				aProto->isa = protocol_class;
				break;
			case protocol_version_objc2:
				aProto->isa = protocol_class2;
				break;
		}
		// Initialize all of the protocols that this protocol refers to
		if (NULL != aProto->protocol_list)
		{
			init_protocols(aProto->protocol_list);
		}
		// Replace this protocol with a unique version of it.
		protocols->list[i] = unique_protocol(aProto);
	}
	return YES;
}

void objc_init_protocols(struct objc_protocol_list *protocols)
{
	if (!init_protocols(protocols))
	{
		set_buffered_object_at_index(protocols, buffered_objects++);
		return;
	}
	if (buffered_objects > 0) { return; }

	// If we can load one protocol, then we can load all of them.
	for (unsigned i=0 ; i<buffered_objects ; i++)
	{
		struct objc_protocol_list *c = buffered_object_at_index(i);
		if (NULL != c)
		{
			init_protocols(c);
			set_buffered_object_at_index(NULL, i);
		}
	}
	compact_buffer();
}

// Public functions:
Protocol *objc_getProtocol(const char *name)
{
	return (Protocol*)protocol_for_name(name);
}

BOOL protocol_conformsToProtocol(Protocol *p, Protocol *other)
{

	return NO;
}

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *p,
	BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int *count)
{
	*count = 0;
	return NULL;
}

Protocol **protocol_copyProtocolList(Protocol *p, unsigned int *count)
{
	*count = 0;
	return NULL;
}

const char *protocol_getName(Protocol *p)
{
	if (NULL != p)
	{
		return p->name;
	}
	return NULL;
}

BOOL protocol_isEqual(Protocol *p, Protocol *other)
{
	if (NULL == p || NULL == other)
	{
		return NO;
	}
	if (p == other || 
		p->name == other->name ||
		0 == strcmp(p->name, other->name))
	{
		return YES;
	}
	return NO;
}

