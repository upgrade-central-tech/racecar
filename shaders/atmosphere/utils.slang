implementing atmosphere;

func safe_sqrt( value: float )->float { return sqrt( max( value, 0.f ) ); }

func clamp_distance( distance: float )->float { return max( distance, 0.f ); }

/// Maps `[0, 1]` to texture space and centers samples in texel centers.
func normalize_to_uv( x: float, texture_size: int )->float
{
    return 0.5f / (float)texture_size + x * ( 1.f - 1.f / (float)texture_size );
}

func clamp_radius( in atms: Parameters, r: float )->float
{
    return clamp( r, atms.bottom_radius, atms.top_radius );
}

func clamp_cos( mu: float )->float { return clamp( mu, -1.f, 1.f ); }


