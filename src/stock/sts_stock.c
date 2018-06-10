
#include <sts_stock.h>

// static int CQgs,CQpg,CQpx;
// int GetIfCQ(int bdate,int edate,sRights cqs)
// { if(cqs.Cqrq>bdate&&cqs.Cqrq<=edate)  return 1;
//   else return 0;
// }
// void GetCqPara(sRights cqs)
// {
//   CQgs=1000;
//   CQpg=CQpx=0;
//   if(cqs.Pxs)  CQpx=cqs.Pxs/10;
//   if(cqs.Sgs>0||cqs.Pgs>0)
//   { CQgs=1000+cqs.Sgs/10+cqs.Pgs/10;
//     CQpg=-cqs.Pgj*cqs.Pgs/10000;
//   }
// }
// int CaclCqPrc(int prc,int way)
// { //formula:  prc=prc*gs+gs*px+pg=(prc+px)*gs+pg;
//   //formula:  prc=prc*gs+px+pg;  //???  which is right
//   //if(way==0)  prc=(prc+CQpx)*CQgs/1000+CQpg;
//   //else        prc=(prc-CQpg)*1000/CQgs-CQpx;
//   if(way==0)  prc=prc*CQgs/1000+CQpg+CQpx;
//   else        prc=(prc-CQpg-CQpx)*1000/CQgs;
//   return prc;
// }
// void RestoreCQdata(sHISDATA *hisd,int all,int now,int begin,int way)  //way=0--forword 1--backword
// { int k;
//   if(way==0)
//   { for(k=now;k<all;k++)
//     { hisd[k].Open=CaclCqPrc(hisd[k].Open,way);
//       hisd[k].High=CaclCqPrc(hisd[k].High,way);
//       hisd[k].Low= CaclCqPrc(hisd[k].Low,way);
//       hisd[k].Close=CaclCqPrc(hisd[k].Close,way);
//     }
//     for(k=begin;k<now;k++)  hisd[k].Volume=(long) ((float)hisd[k].Volume*CQgs/1000);
//   }
//   else if(way==1)
//   { for(k=begin;k<now;k++)
//     { hisd[k].Open=CaclCqPrc(hisd[k].Open,way);
//       hisd[k].High=CaclCqPrc(hisd[k].High,way);
//       hisd[k].Low= CaclCqPrc(hisd[k].Low,way);
//       hisd[k].Close=CaclCqPrc(hisd[k].Close,way);
//     }
//     for(k=now;k<all;k++) hisd[k].Volume=(long) ((float)hisd[k].Volume*1000/CQgs);
//   }
// }
inline void _sts_exright_getpara(s_stock_right *right_, s_stock_right_para *para_)
{
	para_->volgap = 1000; 
	para_->allot  = 0;  //配股
	para_->bonus  = 0;  //红利 
	if (right_->pxs)  para_->bonus = right_->pxs / 10;
	if (right_->sgs>0 || right_->pgs>0)
	{
		para_->volgap = 1000 + right_->sgs / 10 + right_->pgs / 10;
		para_->allot = -1*right_->pgj * right_->pgs / 10000;
	}
};
inline uint32 _sts_exright_get_price(uint32 price, int coinunit_, s_stock_right_para *para_, int mode) 
{
	int coinunit = coinunit_;
	if (coinunit == 0) coinunit = 100;
    int zoom = 1000 / coinunit;
	if (mode == STS_EXRIGHT_FORWORD)
	{
		price = (uint32)((double)(price*zoom - para_->allot - para_->bonus) * 1000 / para_->volgap);
	}
	else{
		price = (uint32)((double)(price*zoom*para_->volgap) / 1000 + para_->allot + para_->bonus);
	}
	return (uint32)((double)price / zoom + 0.5);
}

inline uint32 _sts_exright_get_vol(uint32 vol, s_stock_right_para *para_, int mode)
{
    if (mode == STS_EXRIGHT_FORWORD)
    {
        return (uint32)(vol * para_->volgap / 1000);
    } else {
        return (uint32)(vol * 1000 / para_->volgap);
    }
}

bool _sts_isright(int start_, int stop_, int rightdate) 
{
    int stop  = stop_;
    int start = start_;
    if (start_ > stop_) {
        stop = start_;
        start = stop_;
    }
	if (rightdate>start&&rightdate <= stop)
	{
        return true;
    }	
    return false;
};
uint32 sts_stock_exright_vol(uint32 now_, uint32 stop_, uint32 vol_, s_sts_struct_list *rights_)
{
    int mode = STS_EXRIGHT_FORWORD;
    if (stop_ < now_) {
        mode = STS_EXRIGHT_BEHAND;
    }
    uint32 vol = vol_;
    s_stock_right_para para;
	s_stock_right *right_ps;

	for (int k = 0; k < rights_->count; k++)
	{
		right_ps = sts_struct_list_get(rights_, k);
        if(!_sts_isright(now_, stop_, right_ps->time)) continue;
		_sts_exright_getpara(right_ps, &para);
		vol = _sts_exright_get_vol(vol_, &para, mode); 
	} 
    return vol;

}
uint32 sts_stock_exright_price(uint32 now_, uint32 stop_, uint32 price_,int coinunit_, s_sts_struct_list *rights_)
{
    int mode = STS_EXRIGHT_FORWORD;
    if (stop_ < now_) {
        mode = STS_EXRIGHT_BEHAND;
    }   
    uint32 price = price_;
    s_stock_right_para para;
	s_stock_right *right_ps;

	for (int k = 0; k < rights_->count; k++)
	{
		right_ps = sts_struct_list_get(rights_, k);
        if(!_sts_isright(now_, stop_, right_ps->time)) continue;
		_sts_exright_getpara(right_ps, &para);
		price = _sts_exright_get_price(price_, coinunit_, &para, mode); //open
	} 
    return price;
}

// inline void sts_exright_trans(c_minute_array *days_, int coinunit, s_stock_right *right_ps, int mode, int start, int end) {
// 	//  if(coinunit===undefined) coinunit=100;
// 	int exright_gs, exright_pg, exright_px;
// 	cl_exright_getpara(right_ps, exright_gs, exright_pg, exright_px);
// 	s_stock_day *cur_day;
// 	if (mode == EXRIGHT_FORWORD)
// 	{
// 		for (int i = start; i<end; i++)
// 		{
// 			cur_day = days_->get_item(i);
// 			cur_day->m_open_dw = cl_exright_getprice(cur_day->m_open_dw, coinunit, exright_gs, exright_pg, exright_px, mode); //open
// 			cur_day->m_high_dw = cl_exright_getprice(cur_day->m_high_dw, coinunit, exright_gs, exright_pg, exright_px, mode); //high
// 			cur_day->m_low_dw = cl_exright_getprice(cur_day->m_low_dw, coinunit, exright_gs, exright_pg, exright_px, mode); //low
// 			cur_day->m_new_dw = cl_exright_getprice(cur_day->m_new_dw, coinunit, exright_gs, exright_pg, exright_px, mode); //new
// 			cur_day->m_volume_z = (int64)(cur_day->m_volume_z.aslong() * exright_gs / 1000);
// 		}
// 	}
// 	else{

// 	}
// };


// inline void sts_exright_day(c_minute_array *days_, int coinunit, int mode, int start, int end) {
// 	s_buffer_list *rights = days_->get_right();
// 	if (rights->count<1 || days_->getsize()<1) return ;
// 	if (mode < EXRIGHT_FORWORD || mode > EXRIGHT_BEHAND ) mode = EXRIGHT_FORWORD;
// 	if (start < 0 || start>days_->getsize() - 1) start = 0;
// 	if (end<0) end = days_->getsize() - 1;

// 	s_stock_day *day_ps,*pre_day_ps;
// 	s_stock_right *right_ps;
// 	if (mode == EXRIGHT_FORWORD)
// 	{
// 		for (int i = start; i <= end; i++)
// 		{
// 			for (int j = 0; j<rights->count; j++)
// 			{
// 				if (i<1) continue;
// 				day_ps = (s_stock_day*)days_->get_item(i);
// 				pre_day_ps = (s_stock_day*)days_->get_item(i-1);
// 				right_ps = (s_stock_right*)buffer_list_get(rights,j);
// 				if (cl_isright((uint32)pre_day_ps->m_time_t, (uint32)day_ps->m_time_t, right_ps->m_date_dw))
// 				{
// 					cl_exright_trans(days_, coinunit, right_ps, mode, start, i);
// 					break;
// 				}
// 			}
// 		}
// 	}
// 	else if (mode == EXRIGHT_BEHAND)
// 	{

// 	}
// };

// inline void cl_exright_min(c_minute_array *days_, int coinunit, int mode, int start, int end) 
// {
// 	s_buffer_list *rights = days_->get_right();
// 	if (rights->count<1 || days_->getsize()<1) return;
// 	if (mode < EXRIGHT_FORWORD || mode > EXRIGHT_BEHAND) mode = EXRIGHT_FORWORD;
// 	if (start < 0 || start>days_->getsize() - 1) start = 0;
// 	if (end<0) end = days_->getsize() - 1;

// 	s_stock_day *day_ps, *pre_day_ps;
// 	s_stock_right *right_ps;
// 	if (mode == EXRIGHT_FORWORD)
// 	{
// 		for (int i = start; i <= end; i++)
// 		{
// 			for (int j = 0; j<rights->count; j++)
// 			{
// 				if (i<1) continue;
// 				day_ps = (s_stock_day*)days_->get_item(i);
// 				pre_day_ps = (s_stock_day*)days_->get_item(i - 1);
// 				right_ps = (s_stock_right*)buffer_list_get(rights, j);
// 				if (cl_isright(cl_getdate(pre_day_ps->m_time_t), cl_getdate(day_ps->m_time_t), right_ps->m_date_dw))
// 				{
// 					cl_exright_trans(days_, coinunit, right_ps, mode, start, i);
// 					break;
// 				}
// 			}
// 		}
// 	}
// 	else if (mode == EXRIGHT_BEHAND)
// 	{

// 	}
// };
