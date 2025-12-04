module helpers;

/// Maps `[0, 1]` to texture space and centers samples in texel centers.
public func normalize_to_uv( x: float, texture_size: int )->float
{
    return 0.5f / (float)texture_size + x * ( 1.f - 1.f / (float)texture_size );
}


