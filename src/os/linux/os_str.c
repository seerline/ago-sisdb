
#include <os_str.h>
#include <os_malloc.h>

char *sis_strcat(char *out_, size_t *olen_, const char *in_)
{
	size_t ilen = strlen(in_);
	if (!out_)
	{
		*olen_ = ilen + 1024;
		out_ = (char *)sis_malloc(*olen_);
		memmove(out_, in_, ilen);
		out_[ilen] = 0;
	}
	else
	{
		size_t olen = strlen(out_);
		if (olen + ilen >= *olen_)
		{
			*olen_ = olen + ilen + 1024;
			char *out = (char *)sis_malloc(*olen_);
			memmove(out, out_, olen);
			out[olen] = 0;
			sis_free(out_);
			out_ = out;
		}
		memmove(&out_[olen], in_, ilen);
		out_[olen + ilen] = 0;
	}	
	return out_;
}
