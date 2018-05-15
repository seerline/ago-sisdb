
#include "mword.h"

zint32::zint32(int n)
{
	m_nMul = 0;
	while (n>ZINT32_MAXBASE || n<-ZINT32_MAXBASE)
	{
		m_nMul++;
		n /= 16;
	}
	m_nBase = n;
}
	
zint32::zint32(int nBase,uint32 dwMul)
{
	m_nBase = nBase;
	m_nMul = dwMul;
}

int64 zint32::aslong() const
{
	int64 n = m_nBase;
	for(uint32 i=0;i<m_nMul;i++)
		n *= 16;
	return n;
}

int64 zint32::GetABSValue() const 
{
	int64 n = aslong();
	if(n<0)
		n = -n;
	return n;
}

zint32 zint32::operator-=(const zint32& d)
{
	int64 n = d;
	*this -= n;
	return *this;
}

zint32 zint32::operator-=(const int64 d)
{
	int64 n = *this;
	n -= d;
	*this = n;
	return *this;
}

zint32 zint32::operator+=(const zint32& d)
{
	int64 n = d;
	*this += n;
	return *this;
}

zint32 zint32::operator+=(const int64 d)
{
	int64 n = *this;
	n += d;
	*this = n;
	return *this;
}

zint32 zint32::operator*=(const zint32& d)
{
	int64 n = d;
	*this *= n;
	return *this;
}

zint32 zint32::operator*=(const int64 d)
{
	int64 n = *this;
	n *= d;
	*this = n;
	return *this;
}

zint32 zint32::operator/=(const zint32& d)
{
	int64 n = d;
	*this /= n;
	return *this;
}

zint32 zint32::operator/=(const int64 d)
{
	int64 n = *this;
	n /= d;
	*this = n;
	return *this;
}

int64 zint32::operator+(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n+m;
}

int64 zint32::operator-(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n-m;
}

int64 zint32::operator*(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n*m;
}

int64 zint32::operator/(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n/m;
}

int64 zint32::operator+(const int d) const
{
	int64 n = *this;
	return n+d;
}

int64 zint32::operator-(const int d) const
{
	int64 n = *this;
	return n-d;
}

int64 zint32::operator*(const int d) const
{
	int64 n = *this;
	return n*d;
}

int64 zint32::operator/(const int d) const
{
	int64 n = *this;
	return n/d;
}

zint32 zint32::operator=(const zint32& d)
{
	m_nMul = d.m_nMul;
	m_nBase = d.m_nBase;
	return *this;
}

zint32 zint32::operator=(const int64 n)
{
	int64 d = n;
	int nInc = 0;
	m_nMul = 0;
	while (d + nInc>ZINT32_MAXBASE || d + nInc<-ZINT32_MAXBASE)
	{
		nInc = (d%16)>=8;	//ËÄÉáÎåÈë
		d /= 16;
		m_nMul++;
		if(m_nMul==3)
			break;
	}
	m_nBase = (int)(d+nInc);
	return *this;
}

bool zint32::operator==(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n==m;
}

bool zint32::operator==(const int64 d) const
{
	int64 n = *this;
	return n==d;
}

bool zint32::operator==(const int d) const
{
	int64 n = *this;
	return n==d;
}

bool zint32::operator!=(const zint32& d) const
{
	int64 n = *this;
	int64 m = d;
	return n!=m;
}

bool zint32::operator!=(const int64 d) const
{
	int64 n = *this;
	return n!=d;
}

bool zint32::operator!=(const int d) const
{
	int64 n = *this;
	return n!=d;
}

bool zint32::operator>(int d) const
{
	int64 n = *this;
	return n>d;
}

bool zint32::operator>(zint32 d) const
{
	int64 n = *this;
	int64 m = d;
	return n>m;
}

bool zint32::operator<(int d) const
{
	int64 n = *this;
	return n<d;
}

bool zint32::operator<(zint32 d) const
{
	int64 n = *this;
	int64 m = d;
	return n<m;
}

uint32 zint32::GetRawData()
{
	return *(uint32*)this;
}

void zint32::SetRawData(uint32 dw)
{
	uint32* p = (uint32*)this;
	*p = dw;
}
