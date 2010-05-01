#import "objc/objc-api.h"
#import "objc/blocks_runtime.h"
#include <assert.h>

struct objc_class _NSConcreteGlobalBlock;
struct objc_class _NSConcreteStackBlock;

static struct objc_class _NSConcreteGlobalBlockMeta;
static struct objc_class _NSConcreteStackBlockMeta;

static struct objc_class _NSBlock;
static struct objc_class _NSBlockMeta;

void __objc_update_dispatch_table_for_class(Class);
void __objc_add_class_to_hash(Class);
extern struct sarray *__objc_uninstalled_dtable;
extern objc_mutex_t __objc_runtime_mutex;

static void createNSBlockSubclass(Class superclass, Class newClass, 
		Class metaClass, char *name)
{
	// Initialize the metaclass
	//metaClass->class_pointer = superclass->class_pointer;
	//metaClass->super_class = superclass->class_pointer;
	metaClass->info = _CLS_META | _CLS_RESOLV;
	metaClass->dtable = __objc_uninstalled_dtable;

	// Set up the new class
	newClass->class_pointer = metaClass;
	newClass->super_class = (Class)superclass->name;
	newClass->name = name;
	newClass->info = _CLS_CLASS | _CLS_RESOLV;
	newClass->dtable = __objc_uninstalled_dtable;

	__objc_add_class_to_hash(newClass);

}

#define NEW_CLASS(super, sub) \
	createNSBlockSubclass(super, &sub, &sub ## Meta, #sub)

BOOL objc_create_block_classes_as_subclasses_of(Class super)
{
	if (_NSBlock.super_class != NULL) { return NO; }

	NEW_CLASS(super, _NSBlock);
	NEW_CLASS(&_NSBlock, _NSConcreteStackBlock);
	NEW_CLASS(&_NSBlock, _NSConcreteGlobalBlock);
	return YES;
}
