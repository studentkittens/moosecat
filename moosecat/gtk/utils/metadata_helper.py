from gi.repository import GdkPixbuf


def plyr_cache_to_pixbuf(cache, width=None, height=None):
    '''
    Converts a plyr.Cache to a GdkPixbuf.Pixbuf

    :param cache: A plyr.Cache as obtained from a :class:`moosecat.metadata.Order`
    :param width: Desired width or None if image actual size shall be used
    :param height: Desired height or None if image actual size shall be used

    :raises: An ValueError if cache is not an image (is_image is False)
    :returns: None on Error, a GdkPixbuf.Pixbuf with the image on success.
    '''
    if not cache.is_image:
        raise ValueError('Cache must contain image data!')

    # Decoder for supported image formats
    loader = GdkPixbuf.PixbufLoader()
    loader.set_size(width, height)

    try:
        # Both calls might raise a PixbufError.
        loader.write(cache.data)
        loader.close()
    except GdkPixbuf.PixbufError as err:
        logging.warning('Could not decode image-cache: ' + str(err))
    else:
        return loader.get_pixbuf()
