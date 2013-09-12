from gi.repository import Pango, PangoCairo


def draw_center_text(ctx, width, height, text, font_size=15):
    '''Draw a text at the center of width and height

    :param ctx: a cairo Context to draw to
    :param width: width in pixel that we may draw on
    :param height: height in pixel that we may draw on
    :param text: Actual text as unicode string
    :param font_size: Size of the font to take
    '''
    layout = PangoCairo.create_layout(ctx)
    font = Pango.FontDescription()
    font.set_size(font_size * Pango.SCALE)
    layout.set_font_description(font)
    layout.set_text(text, -1)

    fw, fh = [num / Pango.SCALE / 2 for num in layout.get_size()]
    ctx.move_to(width / 2 - fw, height / 2 - fh)
    PangoCairo.show_layout(ctx, layout)
