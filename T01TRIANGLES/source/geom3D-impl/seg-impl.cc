
#include "geom3D.hh"

// have strange linkage error with this:
// using namespace geom3D;

namespace geom3D
{

Segment::Segment( const Point& A, const Point& B ) : A_ (A), B_ (B),
    sqLen_ (sqDst (A, B))
{}

bool Segment::isValid() const
{
    return A_.isValid () && B_.isValid ();
}

bool Segment::linearContains( const Point& P ) const
{
    return sqLen_ + FP_CMP_PRECISION >= sqDst (P, A_) &&
           sqLen_ + FP_CMP_PRECISION >= sqDst (P, B_);
}

Point Segment::operator|( const Line& line ) const
{
    return line | *this;
}

} // namespace geom3D