from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.graphics.shapes import Drawing
from reportlab.graphics import renderPDF
from reportlab.graphics.barcode import qr

LABEL_TITLE = "AIRDOS04 - DATA UNIT - service log"
QR_URL = "https://docs.dos.ust.cz/pl/AIRDOS04LOG"

FOOTER_LINES = [
    "Data module for AIRDOS04 detectors. Enter the date and time for each performed operation.",
    "Use this log to track the handling and servicing of the data module.",
    "1) INSERTED – date and time when the data module is inserted into the detector",
    "2) REMOVED – date and time when the data module is removed from the detector",
    "3) ERASED – date and time when stored data are erased from the module.",
    "Make sure the log is complete before returning the module for processing or reinserting it into a detector.",
]

TEXT = "" \
"Data module for AIRDOS04 detectors. Enter the date and time for each performed operation. Use this log to track the handling and servicing of the data module. "

TEXT2 = "" \
"1) INSERTED – date and time when the data module is inserted into the detector " \
"2) REMOVED – date and time when the data module is removed from the detector " \
"3) ERASED – date and time when stored data are erased from the module. " \
"Make sure the log is complete before returning the module for processing or reinserting it into a detector."

def draw_label(c, x, y, w, h, total_rows=11):
    margin = 4 * mm

    c.setLineWidth(0.7)
    c.rect(x, y, w, h)

    header_height = 14 * mm
    c.setFont("Helvetica-Bold", 11)
    c.drawCentredString(x + w / 2.0, y + h - margin - 5 * mm, LABEL_TITLE)

    table_top = y + h - margin - header_height
    footer_height = 26 * mm
    table_bottom = y + margin + footer_height

    col1_w = 6 * mm
    remaining_width = w - col1_w
    col_w = remaining_width / 3.0

    col_x = [
        x,
        x + col1_w,
        x + col1_w + col_w,
        x + col1_w + 2 * col_w,
        x + col1_w + 3 * col_w,
    ]

    c.setLineWidth(0.4)
    row_height = (table_top - table_bottom) / total_rows

    for i in range(total_rows + 1):
        y_line = table_top - i * row_height
        c.line(x, y_line, x + w, y_line)

    for cx in col_x:
        c.line(cx, table_bottom, cx, table_top)

    c.setFont("Helvetica-Bold", 7.5)
    header_y = table_top - row_height / 2 - 2
    c.drawCentredString(col_x[0] + col1_w / 2.0, header_y, "#")
    c.drawCentredString(col_x[1] + col_w / 2.0, header_y, "INSERTED")
    c.drawCentredString(col_x[2] + col_w / 2.0, header_y, "REMOVED")
    c.drawCentredString(col_x[3] + col_w / 2.0, header_y, "ERASED")

    c.setFont("Helvetica", 7)
    for i in range(total_rows - 1):
        y_center = table_top - (i + 1.5) * row_height
        c.drawCentredString(col_x[0] + col1_w / 2.0, y_center, str(i + 1))

    qr_size = 18 * mm
    qr_widget = qr.QrCodeWidget(QR_URL)
    bounds = qr_widget.getBounds()
    bw = bounds[2] - bounds[0]
    bh = bounds[3] - bounds[1]

    d = Drawing(qr_size, qr_size, transform=[qr_size / bw, 0, 0, qr_size / bh, 0, 0])
    d.add(qr_widget)

    qr_x = x + w - margin - qr_size
    qr_y = y + margin

    text_x = x + margin - 1 * mm
    text_y = y + margin + 23 * mm

    font_name = "Helvetica"
    font_size = 6.2
    c.setFont(font_name, font_size)
    line_spacing = font_size + 1
    max_text_width = w - 2 * margin - qr_size - 2 * mm

    def wrap_line(text):
        words = text.split(" ")
        lines = []
        current = ""
        for word in words:
            candidate = word if not current else current + " " + word
            if c.stringWidth(candidate, font_name, font_size) <= max_text_width:
                current = candidate
            else:
                if current:
                    lines.append(current)
                current = word
        if current:
            lines.append(current)
        return lines

    # Split long lines and remove unsupported arguments from drawText
    # Instead, use drawString for each line after wrapping

    c.drawString(
        text_x,
        text_y,
        "Data module for AIRDOS04 detectors. Enter the date and time for"
    )
    c.drawString(
        text_x,
        text_y - line_spacing,
        "each performed operation. Use this log to track the handling and"
    )
    c.drawString(
        text_x,
        text_y - 2 * line_spacing,
        "servicing of the data module."
    )

    c.drawString(
        text_x,
        text_y - 3.5 * line_spacing,
        "1) INSERTED – date and time when the module"
    )
    c.drawString(
        text_x,
        text_y - 4.5 * line_spacing,
        "is inserted into the detector"
    )
    c.drawString(
        text_x,
        text_y - 6 * line_spacing,
        "2) REMOVED – date and time when the module"
    )
    c.drawString(
        text_x,
        text_y - 7 * line_spacing,
        "is removed from the detector"
    )
    c.drawString(
        text_x,
        text_y - 8.5 * line_spacing,
        "3) ERASED – date and time when stored data"
    )
    c.drawString(
        text_x,
        text_y - 9.5 * line_spacing,
        "are erased from the module."
    )


    renderPDF.draw(d, c, qr_x, qr_y)

def create_single_label_pdf(path):
    c = canvas.Canvas(path, pagesize=A4)
    page_w, page_h = A4

    label_w = 74 * mm
    label_h = 130 * mm

    x = (page_w - label_w) / 2.0
    y = (page_h - label_h) / 2.0

    draw_label(c, x, y, label_w, label_h, total_rows=11)
    c.showPage()
    c.save()

def create_2up_label_sheet_pdf(path):
    c = canvas.Canvas(path, pagesize=landscape(A4))
    page_w, page_h = landscape(A4)

    label_w = 74 * mm
    label_h = 130 * mm

    cols = 2
    rows = 1
    spacing = 10 * mm  # Space between labels
    top_margin = 10 * mm  # Space above the top edge

    total_width = cols * label_w + (cols - 1) * spacing
    x_start = (page_w - total_width) / 2.0

    for row in range(rows):
        for col in range(cols):
            x = x_start + col * (label_w + spacing)
            y = page_h - (row + 1) * label_h - top_margin  # Adjust y position for top margin
            draw_label(c, x, y, label_w, label_h, total_rows=11)

    c.showPage()
    c.save()

def create_8up_label_sheet_pdf(path):
    c = canvas.Canvas(path, pagesize=landscape(A4))
    page_w, page_h = landscape(A4)

    label_w = 74 * mm
    label_h = 105 * mm

    cols = 4
    rows = 2

    for row in range(rows):
        for col in range(cols):
            x = col * label_w
            y = page_h - (row + 1) * label_h
            draw_label(c, x, y, label_w, label_h, total_rows=11)

    c.showPage()
    c.save()

single_path = "../airdos_label_single_portrait.pdf"
dual_sheet_path = "../airdos_label_2up_landscape.pdf"
sheet_path = "../airdos_label_8up_landscape.pdf"

create_single_label_pdf(single_path)
create_2up_label_sheet_pdf(dual_sheet_path)
create_8up_label_sheet_pdf(sheet_path)
