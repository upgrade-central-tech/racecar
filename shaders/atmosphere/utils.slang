implementing atmosphere;

func safe_sqrt( value: float )->float { return sqrt( max( value, 0.f ) ); }

func clamp_distance( distance: float )->float { return max( distance, 0.f ); }

/// Maps `[0, 1]` to texture space and centers samples in texel centers.
func normalize_to_uv( x: float, texture_size: int )->float
{
    return 0.5f / (float)texture_size + x * ( 1.f - 1.f / (float)texture_size );
}


