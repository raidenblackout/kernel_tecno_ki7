#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/kernel_read_file.h>
#include <linux/vmalloc.h>

#define AUTHOR "Bret Joseph Antonio"
#define DESC "Device Tree variable dumper"

static void walk_properties(struct device_node *node, struct property *child)
{
    struct property *prop;
    const __be32 *p;
    u32 u;
    of_property_for_each_u32(node, child->name, prop, p, u)
    {
        if ((0 == strcmp(prop->name, "model-part-name")) || (0 == strcmp(prop->name, "model-external-name")) ||
            (0 == strcmp(prop->name, "model")) || (0 == strcmp(prop->name, "compatible")) ||
            (0 == strcmp(prop->name, "device_type")) || (0 == strcmp(prop->name, "enable-method")) ||
            (0 == strcmp(prop->name, "entry-method")) || (0 == strcmp(prop->name, "cpu-idle-states")) ||
            (0 == strcmp(prop->name, "status")) || (0 == strcmp(prop->name, "regulator-name")))
        {
            const char *value;
            of_property_read_string(node, prop->name, &value);
            printk(KERN_ERR "%s: %s : %s\n", node->name, prop->name, value);
        }
        else if ((0 == strcmp(prop->name, "orig_dram_info")) || (0 == strcmp(prop->name, "reg")) ||
                 (0 == strcmp(prop->name, "tee_reserved_mem")) || (0 == strcmp(prop->name, "clocks")) ||
                 (0 == strcmp(prop->name, "mblock_info")) || (0 == strcmp(prop->name, "interrupts")) ||
                 (0 == strcmp(prop->name, "cd-gpios")) || (0 == strcmp(prop->name, "pinmux")))
        {
            int index = 0;
            char *message;
            u32 value;
            while (of_property_read_u32_index(node, prop->name, index, &value) == 0)
            {
                if (index == 0)
                {
                    message = (char *)vmalloc(snprintf(NULL, 0, "%u", value) + 1);
                    sprintf(message, "%u", value);
                }
                else
                {
                    size_t size = strlen(message) + snprintf(NULL, 0, "%u", value);
                    char *buffer = (char *)kvrealloc(message, strlen(message), size, GFP_KERNEL);
                    sprintf(buffer, "%s %u", message, value);
                    message = buffer;
                }
                index++;
            }
            printk(KERN_ERR "%s: property name: %s : %s \n", node->name, prop->name, message);
        }
        else if ((0 == strcmp(prop->name, "clock-names")) || (0 == strcmp(prop->name, "pinctrl-names")))
        {
            int index = 0;
            char *message;
            const char *value;
            while (of_property_read_string_index(node, prop->name, index, &value) == 0)
            {
                if (index == 0)
                {
                    message = (char *)vmalloc(snprintf(NULL, 0, "\"%s\"", value) + 1);
                    sprintf(message, "\"%s\"", value);
                }
                else
                {
                    size_t size = strlen(message) + snprintf(NULL, 0, "\"%s\"", value);
                    char *buffer = (char *)kvrealloc(message, strlen(message), size, GFP_KERNEL);
                    sprintf(buffer, "%s \"%s\"", message, value);
                    message = buffer;
                }
                index++;
            }
            printk(KERN_ERR "%s: %s : %s \n", node->name, prop->name, message);
        }
        else if (prop->length == 4 && strcmp(prop->name, "name") != 0)
        {
            u32 value;
            of_property_read_u32(node, prop->name, &value);
            printk(KERN_ERR "%s: %s : %u\n", node->name, prop->name, value);
        }
        else if (prop->length == 8 && strcmp(prop->name, "name") != 0)
        {
            u64 value;
            of_property_read_u64(node, prop->name, &value);
            printk(KERN_ERR "%s : %s : %llu\n", node->name, prop->name, value);
        }
        else if (strcmp(prop->name, "name") != 0)
        {
            if (prop->length != 0)
                printk(KERN_ERR "%s: unknown property: %s, length: %d\n", node->name, prop->name, prop->length);
        }
        prop = NULL;
    }
}

static void find_node(char *path, phandle handle)
{
    struct device_node *node;
    struct device_node *child;
    struct property *prop;
    node = handle == 0 ? of_find_node_by_path(path) : of_find_node_by_phandle(handle);

    if (node)
    {
        for_each_property_of_node(node, prop)
            walk_properties(node, prop);
        for_each_child_of_node(node, child)
        {
            if (handle != 0)
                for_each_property_of_node(child, prop)
                    walk_properties(child, prop);
        }
    }
}

static int __init init(void)
{
    void *data = NULL;
    size_t size;
    char *token;
    char *buffer;
    printk(KERN_ERR "%s\n", "Tree Dumper Start");
    kernel_read_file_from_path("/phandle.txt", 0, &data, ARG_MAX, &size, 0);
    buffer = (char *)data;
    while ((token = strsep(&buffer, "\n")) != NULL)
    {
        phandle address;
        int result = sscanf(token, "%u", &address);
        if ((result == 0) || (address == 0))
        {
            kernel_read_file_from_path("/list.txt", 0, &data, ARG_MAX, &size, 0);
            buffer = (char *)data;
            while ((token = strsep(&buffer, "\n")) != NULL)
            {
                find_node(token, 0);
            }
            break;
        }
        else
        {
            find_node("/", address);
        }
    }
    vfree(data);
    printk(KERN_ERR "%s\n", "Tree Dumper Stop");
    return 0;
}

static void __exit deinit(void)
{
    printk(KERN_ERR "Tree Dumper End\n");
}

module_init(init);
module_exit(deinit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESC);
