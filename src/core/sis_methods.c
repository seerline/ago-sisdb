
#include <sis_methods.h>
#include <sis_sds.h>

/////////////////////////////////
//  s_sis_methods
/////////////////////////////////

s_sis_methods *sis_methods_create(s_sis_method *methods_, int count_)
{
	s_sis_map_pointer *map = sis_map_pointer_create();
	for (int i = 0; i < count_; i++)
	{
		struct s_sis_method *c = methods_ + i;
        s_sis_sds key = sis_sdsempty();
        if (c->style)
        {
		    key = sis_sdscatfmt(key, "%s.%s", c->style, c->command);
        }
        else
        {
            key = sis_sdscatfmt(key, "%s", c->command);
        }        
 		int o = sis_dict_add(map, key, c);
		assert(o == DICT_OK);
	}
	return map;
}
void sis_methods_destroy(s_sis_methods *map_)
{
    sis_map_pointer_destroy(map_);
}

s_sis_method *sis_methods_get(s_sis_methods *map_, const char *name_, const char *style_)
{
	s_sis_method *method = NULL;
	if (map_)
	{
        s_sis_sds key = sis_sdsempty();
        if (style_)
        {
		    key = sis_sdscatfmt(key, "%s.%s", style_, name_);
        }
        else
        {
            key = sis_sdscatfmt(key, "%s", name_);
        }        
		method = (s_sis_method *)sis_dict_fetch_value(map_, key);
		sis_sdsfree(key);
	}
	return method;
}



