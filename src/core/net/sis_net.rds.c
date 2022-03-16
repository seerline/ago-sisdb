#include <sis_net.rds.h>
#include <sis_math.h>

// static void _calc_sds_to_buffer(s_sis_sds s_, int *count_, size_t *size_) 
// {
//     if (s_)
//     {
//         int len = sis_sdslen(s_);
//         if (len > 0)
//         {
//             *count_ = *count_ + 1;
//             *size_ = *size_ + 1 + sis_sqrt10(len) + 2 + len + 2;
//         }
//     }
// }

// static s_sis_sds _pack_sds_to_buffer(s_sis_sds s_, s_sis_sds cmd) 
// {
//     if (s_)
//     {
//         int len = sis_sdslen(s_);
//         if (len > 0)
//         {
//             cmd = sis_sdscatfmt(cmd, "$%u\r\n", len);
//             cmd = sis_sdscatlen(cmd, s_, len);
//             cmd = sis_sdscatlen(cmd, "\r\n", sizeof("\r\n") - 1);
//         }
//     }
//     return cmd;
// }

// s_sis_sds _sis_net_pack_tcp_ask(s_sis_net_message *in_)
// {
//     s_sis_sds cmd;
//     size_t size = 0;
//     int count = 0;

//     _calc_sds_to_buffer(in_->cmd, &count, &size);
//     _calc_sds_to_buffer(in_->key, &count, &size);
//     _calc_sds_to_buffer(in_->argv, &count, &size);

//     {
//         s_sis_list_node *node = in_->argvs;
//         while (node != NULL)
//         {
//             _calc_sds_to_buffer((s_sis_sds)node->value, &count, &size);
//             node = node->next;
//         };
//     }
//     size += 1 + sis_sqrt10(count) + 2;

//     cmd = sis_sdsempty();
//     /* we already know how much storage we need */
//     cmd = sis_sds_addlen(cmd, size);
//     if (cmd == NULL)
//     {
//         return NULL;
//     }

//     /* ccnstruct cmd */
//     cmd = sis_sdscatfmt(cmd, "*%i\r\n", count);
//     cmd = _pack_sds_to_buffer(in_->cmd, cmd);
//     cmd = _pack_sds_to_buffer(in_->key, cmd);
//     cmd = _pack_sds_to_buffer(in_->argv, cmd);
//     {
//         s_sis_list_node *node = in_->argvs;
//         while (node != NULL)
//         {
//             cmd = _pack_sds_to_buffer((s_sis_sds)node->value, cmd);
//             node = node->next;
//         };
//     }

//     assert(sis_sdslen(cmd)==size);

//     return cmd;
// }
// ///////////
// static void _calc_sds_to_sub(s_sis_sds s_, int *count_, size_t *size_) 
// {
//     if (s_)
//     {
//         int len = sis_sdslen(s_);
//         if (len > 0)
//         {
//             *count_ = *count_ + 1;
//             *size_ = *size_ + 2 + len + 2;
//             return ;
//         }
//     }
//     *count_ = *count_ + 1;
//     *size_ = *size_ + 1 + 1 + 2;  // +*\r\n
// }
// static s_sis_sds _pack_sds_to_sub(s_sis_sds sub_, s_sis_sds cmd) 
// {
//     if (sub_)
//     {
//         int len = sis_sdslen(sub_);
//         if (len > 0)
//         {
//             cmd = sis_sdscatfmt(cmd, "++%s\r\n", sub_);
//             return cmd;
//         }
//     }
//     cmd = sis_sdscatlen(cmd, "++*\r\n", 5);  // == all    
//     return cmd;
// }
// s_sis_sds _sis_net_pack_tcp_ans_items(s_sis_net_message *in_, s_sis_sds cmd)
// {
//     size_t size = 0;
//     int count = 0;

//     if (in_->subpub)
//     {
//         _calc_sds_to_sub(in_->key, &count, &size);
//     }
//     {
//         s_sis_list_node *node = in_->rlist;
//         while (node != NULL)
//         {
//             _calc_sds_to_buffer((s_sis_sds)node->value, &count, &size);
//             node = node->next;
//         };
//     }
//     size += 1 + sis_sqrt10(count) + 2;

//     // cmd = sis_sdsempty();
//     /* we already know how much storage we need */
//     cmd = sis_sds_addlen(cmd, size);
//     if (cmd == NULL)
//     {
//         return NULL;
//     }

//     /* ccnstruct command */
//     cmd = sis_sdscatfmt(cmd, "*%i\r\n", count);
//     if (in_->subpub)
//     {
//         _pack_sds_to_sub(in_->key, cmd);
//     }
//     {
//         s_sis_list_node *node = in_->rlist;
//         while (node != NULL)
//         {
//             cmd = _pack_sds_to_buffer((s_sis_sds)node->value, cmd);
//             node = node->next;
//         };
//     }

//     assert(sis_sdslen(cmd)==size);

//     return cmd;

// }
// s_sis_sds _sis_net_pack_tcp_ans_item(s_sis_net_message *in_, s_sis_sds cmd)
// {
//     if (!in_->subpub)
//     {
//         size_t len = sis_sdslen(in_->rval);
//         cmd = sis_sdscatfmt(cmd, "$%u\r\n", len);
//         cmd = sis_sdscatlen(cmd, in_->rval, len);
//         cmd = sis_sdscatlen(cmd, "\r\n", sizeof("\r\n") - 1);
//     }
//     else
//     {
//         size_t size = 0;
//         int count = 0;

//         _calc_sds_to_sub(in_->key, &count, &size);
//         _calc_sds_to_buffer(in_->rval, &count, &size);

//         size += 1 + sis_sqrt10(count) + 2;

//         // cmd = sis_sdsempty();
//         cmd = sis_sds_addlen(cmd, size);
//         if (cmd == NULL)
//         {
//             return NULL;
//         }
//         /* construct command */
//         cmd = sis_sdscatfmt(cmd, "*%i\r\n", count);
//         cmd = _pack_sds_to_sub(in_->key, cmd);
//         cmd = _pack_sds_to_buffer(in_->rval, cmd);
//     }
//     return cmd;
// }

// s_sis_sds _sis_net_pack_tcp_ans(s_sis_net_message *in_)
// {
//     s_sis_sds cmd = sis_sdsempty();

//     switch (in_->style)
//     {
//     case SIS_NET_REPLY_STRING:
//         cmd = _sis_net_pack_tcp_ans_item(in_, cmd);
//         break;
//     case SIS_NET_REPLY_ARRAY:
//         cmd = _sis_net_pack_tcp_ans_items(in_, cmd);
//         break;
//     case SIS_NET_REPLY_INTEGER:
//         cmd = sis_sdscatfmt(cmd, ":%I\r\n", in_->rint);
//         break;
//     case SIS_NET_REPLY_INFO:
//         cmd = sis_sdscatfmt(cmd, "+%s\r\n", in_->rval);
//         break;
//     case SIS_NET_REPLY_ERROR:
//         cmd = sis_sdscatfmt(cmd, "-%s\r\n", in_->rval);
//         break;
//     default:
//         return NULL;
//     }
//     return cmd;   
// }
// s_sis_sds sis_net_pack_rds(struct s_sis_net_class *sock_, s_sis_net_message *in_)
// {
//     s_sis_sds out = NULL;
//     if (in_->style < SIS_NET_REPLY_MESSAGE)
//     {
//         out = _sis_net_pack_tcp_ask(in_);
//     }
//     else
//     {
//         out = _sis_net_pack_tcp_ans(in_);
//     }
//     return out;
// }

//////////////////////////////////////////////////////////////////////
//  unpack -- 
//////////////////////////////////////////////////////////////////////

// s_sis_sds _sis_net_unpack_tcp_item(s_sis_sds str_, s_sis_memory* in_)
// {
//     if (str_)
//     {
//         sis_sdsfree(str_);
//         str_ = NULL;
//     }
//     size_t len;
//     char *ptr = sis_memory_read_line(in_, &len);
//     if (!ptr || len < 1 || ptr[0]!='$')
//     {
//         return NULL;
//     }
//     size_t size = sis_str_read_long(&ptr[1]);
//     if (size > 0 && sis_memory_get_size(in_) >= (len + 2 + size + 2))
//     {
//         sis_memory_move(in_, len + 2); // \r\n
//         str_ = sis_sdsnewlen(sis_memory(in_), size); 
//         sis_memory_move(in_, size + 2);
//     }  
//     return str_;   
// }

// int _sis_net_unpack_tcp_ask_array(s_sis_net_message *out_, s_sis_memory* in_)
// {
//     size_t offset = sis_memory_get_address(in_);

//     size_t len;
//     char  *ptr = sis_memory_read_line(in_, &len);
//     int    count = sis_str_read_long(&ptr[1]);
    
//     sis_memory_move(in_, len + 2); // \r\n    
//     out_->cmd = _sis_net_unpack_tcp_item(out_->cmd, in_);
//     if (!out_->cmd)
//     {
//         goto error;
//     }
//     if (--count == 0)
//     {
//         return 0;
//     }
//     out_->key = _sis_net_unpack_tcp_item(out_->key, in_);
//     if (!out_->key)
//     {
//         goto error;
//     }
//     if (--count == 0)
//     {
//         return 0;
//     }
//     out_->argv = _sis_net_unpack_tcp_item(out_->argv, in_);
//     if (!out_->argv)
//     {
//         goto error;
//     }
//     if (--count == 0)
//     {
//         return 0;
//     }
//     s_sis_sds value = NULL;
//     while(count > 0)
//     {
//         value = _sis_net_unpack_tcp_item(value, in_);
//         if (value)
//         {
//             out_->argvs = sis_sdsnode_push_node_sds(out_->argvs, value);
//             value = NULL;
//         }
//         count--;
//     }    
// error:
//     sis_memory_jumpto(in_, offset);
//     return -1;

// }
// int _sis_net_unpack_tcp_ask(s_sis_net_message *out_, s_sis_memory* in_)
// {
//     int o = -1;
//     size_t len;
//     char *first = sis_memory_read_line(in_, &len);
//     if (first && len > 0)
//     {
//         switch (first[0])
//         {
//         case '+':
//             out_->style = SIS_NET_ASK_INFO;
//             out_->cmd = sis_sdsnewlen(&first[1], len - 1); 
//             sis_memory_move(in_, len + 2); // \r\n
//             o = 0;
//             break;
//         case '$':
//             {
//                 out_->style = SIS_NET_ASK_STRING;
//                 out_->argv = _sis_net_unpack_tcp_item(out_->argv, in_);
//                 if (out_->argv)
//                 {
//                     // 不为空才会移动in位置
//                     o = 0;
//                 }
//             }
//             break;
//         case '*':
//             {
//                 out_->style = SIS_NET_ASK_ARRAY;
//                 o = _sis_net_unpack_tcp_ask_array(out_, in_);
//             }
//             break;
//         default:
//             return -2;
//         }
//     }
//     return o;
// }

// int _sis_net_unpack_tcp_ans_array(s_sis_net_message *out_, s_sis_memory* in_)
// {
//     size_t offset = sis_memory_get_address(in_);

//     size_t len;
//     char  *ptr = sis_memory_read_line(in_, &len);
//     int count = sis_str_read_long(&ptr[1]);
    
//     sis_memory_move(in_, len + 2); // \r\n  

//     if(count > 1)
//     {
//         ptr = sis_memory_read_line(in_, &len);
//         if (len > 2 &&ptr[0] == '+' && ptr[1] == '+')
//         {
//             out_->subpub = 1; // 
//             out_->key = sis_sdsnewlen(&ptr[2], len - 2); 
//             sis_memory_move(in_, len + 2); // \r\n
//             count--;
//         }
//     }
//     if(count == 1)
//     {
//         out_->rval = _sis_net_unpack_tcp_item(out_->rval, in_);
//         if (!out_->rval)
//         {
//             goto error;
//         }
//         return 0;   
//     }
//     else
//     {
//         s_sis_sds value = NULL;
//         while(count > 0)
//         {
//             value = _sis_net_unpack_tcp_item(value, in_);
//             if (value)
//             {
//                 out_->rlist = sis_sdsnode_push_node_sds(out_->rlist, value);
//                 value = NULL;
//             }
//             count--;
//         }      
//     } 
// error:
//     sis_memory_jumpto(in_, offset);
//     return -1;

// }
// int _sis_net_unpack_tcp_ans(s_sis_net_message *out_, s_sis_memory* in_)
// {
//     int o = -1;
//     size_t len;
//     char *first = sis_memory_read_line(in_, &len);
//     if (first && len > 0)
//     {
//         switch (first[0])
//         {
//         case ':':
//             out_->style = SIS_NET_REPLY_INTEGER;
//             out_->rint = sis_str_read_long(&first[1]);
//             sis_memory_move(in_, len + 2); // \r\n
//             o = 0;
//             break;
//         case '+':
//             out_->style = SIS_NET_REPLY_INFO;
//             out_->rval = sis_sdsnewlen(&first[1], len - 1); 
//             sis_memory_move(in_, len + 2); // \r\n
//             o = 0;
//             break;
//         case '-':
//             out_->style = SIS_NET_REPLY_ERROR;
//             out_->rval = sis_sdsnewlen(&first[1], len - 1); 
//             sis_memory_move(in_, len + 2); // \r\n
//             o = 0;
//             break;
//         case '$':
//             {
//                 out_->style = SIS_NET_REPLY_STRING;
//                 out_->argv = _sis_net_unpack_tcp_item(out_->argv, in_);
//                 if (out_->argv)
//                 {
//                     // 不为空才会移动in位置
//                     o = 0;
//                 }
//             }
//             break;
//         case '*':
//             {
//                 out_->style = SIS_NET_REPLY_ARRAY;
//                 o = _sis_net_unpack_tcp_ans_array(out_, in_);
//             }
//             break;
//         default:
//             return -2;
//         }
//     }
//     return o;
// }
// bool sis_net_unpack_rds(s_sis_net_class *sock_, s_sis_net_message *out_, s_sis_memory* in_)
// {
//     int ok = -1;
//     if (out_->style < SIS_NET_REPLY_MESSAGE)
//     {
//         ok = _sis_net_unpack_tcp_ask(out_, in_);
//     }
//     else
//     {
//         ok = _sis_net_unpack_tcp_ans(out_, in_);
//     }
//     return (ok==0);
// }

int sis_net_pack_rds(s_sis_memory* in_, s_sis_net_tail *info,s_sis_memory *out_)
{
    return 1;
}
int sis_net_unpack_rds(s_sis_memory* in_, s_sis_net_tail *info_, s_sis_memory *out_)
{
    // 这个要判断包是否完整了
    return 0;
}

bool sis_net_encoded_rds(s_sis_net_message *in_, s_sis_memory *out_)
{
    return true;
}
bool sis_net_decoded_rds(s_sis_memory *in_, s_sis_net_message *out_)
{
    return true;
}

