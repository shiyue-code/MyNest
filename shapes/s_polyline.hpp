#ifndef POLYLINE_H
#define POLYLINE_H

#include "../s_common.hpp"
#include "s_point.hpp"
#include "s_shape.h"
#include "s_line.hpp"
#include <deque>

namespace S_Shape2D {

template <typename T, typename Container = std::deque<Point<T>>>
class Polyline : public Shape {
public:
    using Point = Point<T>;
    using Line = Line<T>;
    using reference = Point&;
    using const_reference = const Point&;
    using Coord = typename Point::Coord;

    //Container
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using size_type = typename Container::size_type;

    // Shape interface
public:
    ShapeType
    rtti() override
    {
        return ShapeType::ShapePolyline;
    }

public:
    Polyline() { }

    Polyline(const Polyline& poly)
        : ctrlPoints(poly.ctrlPoints)
    {
    }

    Polyline(Polyline&& poly)
        : ctrlPoints(std::move(poly.ctrlPoints))
    {
    }

    iterator begin()
    {
        return ctrlPoints.begin();
    }
    const_iterator begin() const
    {
        return ctrlPoints.begin();
    }

    iterator iteratorAt(int index)
    {
        if (index > (int)size())
            return end();
        auto iter = begin();
        std::advance(iter, index);
        return iter;
    }

    iterator end()
    {
        return ctrlPoints.end();
    }
    const_iterator end() const
    {
        return ctrlPoints.end();
    }

    size_type size() const
    {
        return ctrlPoints.size();
    }

    bool empty() const
    {
        return ctrlPoints.empty();
    }

    void add(const Point& pt)
    {
        ctrlPoints.push_back(pt);
    }

    void insert(const Point& point, size_type index = npos)
    {
        index == npos ? add(point) : (void)ctrlPoints.insert(iteratorAt(index), point);
    }

    void remove(size_type index)
    {
        ctrlPoints.erase(iteratorAt(index));
    }
    void clear()
    {
        ctrlPoints.clear();
    }

    void reverse()
    {
        std::reverse(ctrlPoints.begin(), ctrlPoints.end());
    }

    reference operator[](size_type index)
    {
        return ctrlPoints.at(index);
    }

    const_reference operator[](size_type index) const
    {
        return ctrlPoints.at(index);
    }

    enum PolygonType {
        ConcavePolygon,
        ConvexPolygon,
    };
    int polygonType()
    {
        size_type nSize = this->size();
        for (size_type i = 0; i < nSize; ++i) {
            //        if()
        }
        return 0;
    }

    enum Orientation {
        Clockwise,
        CounterClockwise,
    };
    int orientation()
    {
        return area() > 0 ? CounterClockwise : Clockwise;
    }

    T area()
    {
        size_type nSize = this->size();
        if (nSize < 3)
            return 0;
        double a = 0;
        for (size_type i = 0, j = nSize - 1; i < nSize; ++i) {
            a += ((*this)[j].x + (*this)[i].x) * ((*this)[j].y - (*this)[i].y);
            j = i;
        }
        return -a * 0.5;
    }


    void rotate(T radian)
    {
        for (auto& pt : ctrlPoints) {
            pt.rotate(radian);
        }
    }

    void translate(const Point& ts)
    {
        for (auto& pt : ctrlPoints) {
            pt.translate(ts);
        }
    }

    Polyline& operator=(const Polyline& poly)
    {
        ctrlPoints = poly.ctrlPoints;
        return *this;
    }

    bool operator==(const Polyline& poly) const
    {
        if(this->size()!=poly.size())
            return false;
        else
        {
            for(size_t i = 0 ; i < size();i++)
            {
                if(ctrlPoints[i].x != poly[i].x||
                        ctrlPoints[i].y != poly[i].y)
                    return false;
            }
            return true;
        }
    }
    /********************************************************************************************
    射线法判断点q与多边形polygon的位置关系，要求polygon为简单多边形(不存在自交情况)，顶点逆时针排列
    如果点在多边形内：   返回0
    如果点在多边形边上： 返回1
    如果点在多边形外：    返回2
    *********************************************************************************************/
    int insidepolygon(const Point &q)
    {
        Polyline poly(*this);
        struct SimpleLine
        {
            Point s;
            Point e;
            SimpleLine(Point a, Point b) { s=a; e=b;}

            static double multiply(const Point &sp, const Point &ep,const Point &op)
            {
                return((sp.x-op.x)*(ep.y-op.y)-(ep.x-op.x)*(sp.y-op.y));
            }

            bool intersect(SimpleLine v)
            {
                SimpleLine u(*this);
                return( (std::max(u.s.x,u.e.x)>=std::min(v.s.x,v.e.x))&&                     //排斥实验
                        (std::max(v.s.x,v.e.x)>=std::min(u.s.x,u.e.x))&&
                        (std::max(u.s.y,u.e.y)>=std::min(v.s.y,v.e.y))&&
                        (std::max(v.s.y,v.e.y)>=std::min(u.s.y,u.e.y))&&
                        (multiply(v.s,u.e,u.s)*multiply(u.e,v.e,u.s)>=0)&&         //跨立实验
                        (multiply(u.s,v.e,v.s)*multiply(v.e,u.e,v.s)>=0));
            }
            bool intersect_A(SimpleLine v)
            {
                SimpleLine u(*this);
                return    ((u.intersect(v))&&
                           (!u.online(v.s))&&
                           (!u.online(v.e))&&
                           (!v.online(u.e))&&
                           (!v.online(u.s)));
            }

            bool online(Point p)
            {
                SimpleLine l(*this);
                return( (SimpleLine::multiply(l.e,p,l.s)==0) &&( ( (p.x-l.s.x)*(p.x-l.e.x)<=0 )&&( (p.y-l.s.y)*(p.y-l.e.y)<=0 ) ) );
            }


            SimpleLine() { }
            SimpleLine(const SimpleLine &line){
                this->s = line.s;
                this->e = line.e;
            }
        };

        int c=0;
        SimpleLine l1,l2;
        bool bintersect_a,bonline1,bonline2,bonline3;
        double r1,r2;

        l1.s=q;
        l1.e=q;
        l1.e.x=double(INFINITY);
        size_t n= poly.size();
        for (size_t i=0;i<poly.size();i++)
        {
            l2.s=poly[i];
            l2.e=poly[(i+1)%n];
            if(l2.online(q))
                return 1; // 如果点在边上，返回1
            if ( (bintersect_a=l1.intersect_A(l2))|| // 相交且不在端点
                 ( (bonline1=l1.online(poly[(i+1)%n]))&& // 第二个端点在射线上
                   ( ((!(bonline2=l1.online(poly[(i+2)%n])))&& // 前一个端点和后一个端点在射线两侧
                      ((r1=SimpleLine::multiply(poly[i],poly[(i+1)%n],l1.s)*SimpleLine::multiply(poly[(i+1)%n],poly[(i+2)%n],l1.s))>0)) ||
                     ((bonline3=l1.online(poly[(i+2)%n]))&&     // 下一条边是水平线，前一个端点和后一个端点在射线两侧
                      ((r2=SimpleLine::multiply(poly[i],poly[(i+2)%n],l1.s)*SimpleLine::multiply(poly[(i+2)%n],
                                                                                                 poly[(i+3)%n],l1.s))>0))
                     )
                   )
                 ) c++;
        }
        if(c%2 == 1)
            return 0;
        else
            return 2;
    }

    /********************************************************************************************
    射线法判断线段与多边形polygon的位置关系，要求polygon为简单多边形(不存在自交情况)，顶点逆时针排列
    如果线段与多边形不相交：   返回0
    如果线段与多边形相交：     返回1
    如果线段在多边形内部：     返回2
    *********************************************************************************************/
    int IntersectPolygon(const Line &line)
    {
        Polyline poly(*this);
        size_t size = poly.size();
        for(size_t i = 0 ; i < size; i++)
        {
            Line TmpLine(poly[i],poly[(i+1)%size]);
            if(TmpLine.InterSect(line))
                return 1;
        }
        if(insidepolygon(line.Begin())==0)
            return 2;
        return 0;
    }
    /********************************************************************************************
    射线法判断线段与多边形polygon的位置关系，要求polygon为简单多边形(不存在自交情况)，顶点逆时针排列
    如果多边形与多边形不相交：   返回0
    如果多边形与多边形相交：     返回1
    如果多边形在多边形内部：     返回2
    *********************************************************************************************/
    int Intersect(const Polyline &poly)
    {
        size_t size = poly.size();
        for(size_t i = 0 ; i < poly.size(); i++)
        {
            Line TmpLine(poly[i],poly[(i+1)%size]);
            if(IntersectPolygon(TmpLine)==1) //有一条直线于多边形相交的时候
            {
                return 1;
            }
        }
        if(insidepolygon(poly[0])==0)  //上面判断不存在直线的相交 只要有一个点在内部就说明整个图形都在内部
            return 2;
        return 0;
    }
private:
    Container ctrlPoints;

    static const size_type npos { static_cast<size_type>(-1) };
};

typedef Polyline<double> Polyline2D;
typedef Polyline<float> Polyline2F;
typedef Polyline<Rational64> Polyline2R;
}

USE_S_(Polyline2D);
USE_S_(Polyline2F);
USE_S_(Polyline2R);

#endif // POLYLINE_H
