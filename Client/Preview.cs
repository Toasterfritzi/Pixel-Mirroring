using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace PixelMirroringPreview
{
    public class CustomWindow : Form
    {
        private const int WM_NCHITTEST = 0x0084;
        private const int WM_SIZING    = 0x0214;
        private const int HTCLIENT     = 1;
        private const int HTCAPTION    = 2;
        private const int HTTOPLEFT    = 13;
        private const int HTTOPRIGHT   = 14;
        private const int HTBOTTOMLEFT = 16;
        private const int HTBOTTOMRIGHT= 17;
        private const int HTLEFT       = 10;
        private const int HTRIGHT      = 11;
        private const int HTTOP        = 12;
        private const int HTBOTTOM     = 15;

        private const int WMSZ_LEFT        = 1;
        private const int WMSZ_RIGHT       = 2;
        private const int WMSZ_TOP         = 3;
        private const int WMSZ_TOPLEFT     = 4;
        private const int WMSZ_TOPRIGHT    = 5;
        private const int WMSZ_BOTTOM      = 6;
        private const int WMSZ_BOTTOMLEFT  = 7;
        private const int WMSZ_BOTTOMRIGHT = 8;

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left, Top, Right, Bottom; }

        // Layout rectangles
        private Rectangle rectPhone;
        private Rectangle rectBubble;
        private Rectangle rectDrag, rectMin, rectMax, rectClose;

        // State
        private bool isLandscape = false;
        private bool isMaxHeight = false;
        private Rectangle restoreBounds; // saved bounds before max-height
        private int hoveredButton = -1; // 0=drag, 1=min, 2=max, 3=close

        // Constants
        private const int PHONE_CORNER_RADIUS = 24;
        private const int BUBBLE_CORNER_RADIUS = 18;
        private const int BUBBLE_W = 155;
        private const int BUBBLE_H = 36;
        private const int BUBBLE_GAP = 6;
        private const int MIN_PHONE_W = 140;

        // Windows Segoe MDL2 Assets icon codepoints
        private static readonly string ICON_DRAG     = "\uE700"; // GlobalNavButton (hamburger)
        private static readonly string ICON_MINIMIZE  = "\uE921"; // ChromeMinimize
        private static readonly string ICON_MAXIMIZE  = "\uE922"; // ChromeMaximize
        private static readonly string ICON_RESTORE   = "\uE923"; // ChromeRestore
        private static readonly string ICON_CLOSE     = "\uE8BB"; // ChromeClose

        private Font iconFont;

        public CustomWindow()
        {
            this.FormBorderStyle = FormBorderStyle.None;
            this.DoubleBuffered = true;
            this.SetStyle(ControlStyles.ResizeRedraw | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "Pixel Mirroring";
            this.BackColor = Color.FromArgb(30, 30, 30);
            this.ShowInTaskbar = true;

            // Use Segoe MDL2 Assets for native Windows icons
            iconFont = new Font("Segoe MDL2 Assets", 10f);

            SetPhoneSize(340, 604); // 9:16 ratio
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && iconFont != null) iconFont.Dispose();
            base.Dispose(disposing);
        }

        private void SetPhoneSize(int phoneW, int phoneH)
        {
            int totalW = phoneW;
            int totalH = BUBBLE_H + BUBBLE_GAP + phoneH;
            this.ClientSize = new Size(totalW, totalH);
        }

        private double GetAspectRatio()
        {
            return isLandscape ? 16.0 / 9.0 : 9.0 / 16.0;
        }

        private void RecalcLayout()
        {
            int w = this.ClientSize.Width;
            int h = this.ClientSize.Height;

            int phoneTop = BUBBLE_H + BUBBLE_GAP;
            rectPhone = new Rectangle(0, phoneTop, w, h - phoneTop);

            int bw = Math.Min(BUBBLE_W, w);
            int bubbleX = w - bw;
            rectBubble = new Rectangle(bubbleX, 0, bw, BUBBLE_H);

            int btnW = bw / 4;
            int remainder = bw - btnW * 4;
            rectDrag  = new Rectangle(rectBubble.X, 0, btnW + remainder, BUBBLE_H);
            rectMin   = new Rectangle(rectDrag.Right, 0, btnW, BUBBLE_H);
            rectMax   = new Rectangle(rectMin.Right, 0, btnW, BUBBLE_H);
            rectClose = new Rectangle(rectMax.Right, 0, btnW, BUBBLE_H);
        }

        private void UpdateRegion()
        {
            RecalcLayout();
            Region rgn = new Region(Rectangle.Empty);
            using (GraphicsPath bubblePath = RoundedRect(rectBubble, BUBBLE_CORNER_RADIUS))
                rgn.Union(bubblePath);
            using (GraphicsPath phonePath = RoundedRect(rectPhone, PHONE_CORNER_RADIUS))
                rgn.Union(phonePath);
            this.Region = rgn;
        }

        private GraphicsPath RoundedRect(Rectangle bounds, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            if (bounds.Width <= 0 || bounds.Height <= 0)
            {
                return path; // Return empty path if bounds are invalid (e.g. minimized)
            }
            int d = radius * 2;
            if (d > bounds.Width) d = bounds.Width;
            if (d > bounds.Height) d = bounds.Height;
            if (d <= 0)
            {
                path.AddRectangle(bounds);
                return path;
            }
            
            // Adjust bounds slightly to prevent region clipping the right/bottom edge
            int x = bounds.X;
            int y = bounds.Y;
            int w = bounds.Width;
            int h = bounds.Height;
            
            path.AddArc(x, y, d, d, 180, 90);
            path.AddArc(x + w - d, y, d, d, 270, 90);
            path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
            path.AddArc(x, y + h - d, d, d, 90, 90);
            path.CloseFigure();
            return path;
        }

        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            if (this.WindowState == FormWindowState.Minimized || this.ClientSize.Width <= 0 || this.ClientSize.Height <= 0)
                return; // Do not recalculate regions when minimized
            UpdateRegion();
            this.Invalidate();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            UpdateRegion();
        }

        // ---- Button click handling (manual, not via NCHITTEST) ----

        private int HitTestButton(Point clientPt)
        {
            if (rectClose.Contains(clientPt)) return 3;
            if (rectMax.Contains(clientPt))   return 2;
            if (rectMin.Contains(clientPt))   return 1;
            if (rectDrag.Contains(clientPt))  return 0;
            return -1;
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (e.Button != MouseButtons.Left) return;

            int btn = HitTestButton(e.Location);
            if (btn == 1) // Minimize
            {
                this.WindowState = FormWindowState.Minimized;
            }
            else if (btn == 2) // Max-Height / Restore
            {
                ToggleMaxHeight();
            }
            else if (btn == 3) // Close
            {
                this.Close();
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            int newHover = HitTestButton(e.Location);
            if (newHover != hoveredButton)
            {
                hoveredButton = newHover;
                this.Invalidate(rectBubble);
            }
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);
            if (hoveredButton != -1)
            {
                hoveredButton = -1;
                this.Invalidate(rectBubble);
            }
        }

        private void ToggleMaxHeight()
        {
            if (isMaxHeight)
            {
                // Restore
                isMaxHeight = false;
                this.Bounds = restoreBounds;
            }
            else
            {
                // Save current bounds for restore
                restoreBounds = this.Bounds;
                isMaxHeight = true;

                Rectangle workArea = Screen.FromControl(this).WorkingArea;

                if (isLandscape)
                {
                    // Landscape: take the full screen (like normal maximize)
                    this.Bounds = workArea;
                }
                else
                {
                    // Portrait: only take full height, keep aspect ratio width
                    int phoneH = workArea.Height - BUBBLE_H - BUBBLE_GAP;
                    int phoneW = (int)(phoneH * GetAspectRatio());
                    int totalW = phoneW;
                    int totalH = workArea.Height;

                    // Center horizontally on screen
                    int x = workArea.X + (workArea.Width - totalW) / 2;
                    this.SetBounds(x, workArea.Y, totalW, totalH);
                }
            }
            UpdateRegion();
            this.Invalidate();
        }

        // ---- Keyboard for L/P toggle ----
        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            if (e.KeyCode == Keys.L && !isLandscape)
            {
                isLandscape = true;
                isMaxHeight = false;
                int phoneH = rectPhone.Height;
                int phoneW = (int)(phoneH * (16.0 / 9.0));
                if (phoneW < MIN_PHONE_W) { phoneW = MIN_PHONE_W; phoneH = (int)(phoneW / (16.0 / 9.0)); }
                SetPhoneSize(phoneW, phoneH);
            }
            else if (e.KeyCode == Keys.P && isLandscape)
            {
                isLandscape = false;
                isMaxHeight = false;
                int phoneW = Math.Min(rectPhone.Width, 400);
                int phoneH = (int)(phoneW / (9.0 / 16.0));
                SetPhoneSize(phoneW, phoneH);
            }
        }

        // ---- Painting ----
        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            // ---- Phone Screen ----
            Rectangle drawPhone = rectPhone;
            drawPhone.Inflate(-1, -1); // Prevent border clipping
            
            using (SolidBrush phoneBrush = new SolidBrush(Color.FromArgb(22, 22, 22)))
            using (GraphicsPath phonePath = RoundedRect(drawPhone, PHONE_CORNER_RADIUS))
            {
                g.FillPath(phoneBrush, phonePath);
            }
            using (Pen borderPen = new Pen(Color.FromArgb(65, 65, 65), 1.5f))
            using (GraphicsPath phonePath = RoundedRect(drawPhone, PHONE_CORNER_RADIUS))
            {
                g.DrawPath(borderPen, phonePath);
            }

            // ---- Bubble background ----
            Rectangle drawBubble = rectBubble;
            drawBubble.Inflate(-1, -1);
            
            using (SolidBrush bubbleBrush = new SolidBrush(Color.FromArgb(50, 50, 50)))
            using (GraphicsPath bubblePath = RoundedRect(drawBubble, BUBBLE_CORNER_RADIUS))
            {
                g.FillPath(bubbleBrush, bubblePath);
            }
            using (Pen bubbleBorder = new Pen(Color.FromArgb(75, 75, 75), 1.5f))
            using (GraphicsPath bubblePath = RoundedRect(drawBubble, BUBBLE_CORNER_RADIUS))
            {
                g.DrawPath(bubbleBorder, bubblePath);
            }

            // ---- Button hover highlights ----
            Rectangle[] btnRects = { rectDrag, rectMin, rectMax, rectClose };
            for (int i = 0; i < 4; i++)
            {
                if (hoveredButton == i)
                {
                    Color hoverColor = (i == 3) ? Color.FromArgb(196, 43, 28) : Color.FromArgb(75, 75, 75);
                    using (SolidBrush hoverBrush = new SolidBrush(hoverColor))
                    {
                        // Clip hover highlight to bubble shape
                        Rectangle hr = btnRects[i];
                        // For first and last buttons, round the appropriate corners
                        if (i == 0) // drag = leftmost
                        {
                            using (GraphicsPath hp = RoundedRect(hr, BUBBLE_CORNER_RADIUS))
                                g.FillPath(hoverBrush, hp);
                        }
                        else if (i == 3) // close = rightmost
                        {
                            using (GraphicsPath hp = RoundedRect(hr, BUBBLE_CORNER_RADIUS))
                                g.FillPath(hoverBrush, hp);
                        }
                        else
                        {
                            g.FillRectangle(hoverBrush, hr);
                        }
                    }
                }
            }

            // ---- Button Icons (Segoe MDL2 Assets) ----
            StringFormat sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
            using (SolidBrush iconBrush = new SolidBrush(Color.FromArgb(220, 220, 220)))
            {
                g.DrawString(ICON_DRAG, iconFont, iconBrush, rectDrag, sf);
                g.DrawString(ICON_MINIMIZE, iconFont, iconBrush, rectMin, sf);
                g.DrawString(isMaxHeight ? ICON_RESTORE : ICON_MAXIMIZE, iconFont, iconBrush, rectMax, sf);

                // Close icon turns white on red hover
                SolidBrush closeBrush = (hoveredButton == 3) 
                    ? new SolidBrush(Color.White) 
                    : iconBrush;
                g.DrawString(ICON_CLOSE, iconFont, closeBrush, rectClose, sf);
                if (hoveredButton == 3) closeBrush.Dispose();
            }

            // ---- Phone content (placeholder) ----
            using (Font titleFont = new Font("Segoe UI", 14, FontStyle.Bold))
            using (Font bodyFont = new Font("Segoe UI", 10))
            using (SolidBrush textBrush = new SolidBrush(Color.FromArgb(160, 160, 160)))
            {
                int cy = rectPhone.Y + rectPhone.Height / 2 - 50;
                Rectangle contentRect = new Rectangle(rectPhone.X + 20, cy, rectPhone.Width - 40, 40);
                g.DrawString("\U0001F4F1 Phone Screen", titleFont, textBrush, contentRect, sf);

                contentRect.Y += 45;
                contentRect.Height = 100;
                string mode = isLandscape ? "Landscape (16:9)" : "Portrait (9:16)";
                string maxState = isMaxHeight ? " [Max Height]" : "";
                g.DrawString("Modus: " + mode + maxState + "\n[L] Landscape  [P] Portrait", bodyFont, textBrush, contentRect, sf);
            }
        }

        // ---- WndProc: Hit-testing for drag & resize only ----
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_SIZING)
            {
                RECT r = (RECT)Marshal.PtrToStructure(m.LParam, typeof(RECT));
                int edge = m.WParam.ToInt32();
                int totalW = r.Right - r.Left;
                int totalH = r.Bottom - r.Top;
                int phoneH = totalH - BUBBLE_H - BUBBLE_GAP;
                int phoneW = totalW;
                double ratio = GetAspectRatio();

                int newPhoneW, newPhoneH;
                if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT)
                {
                    newPhoneW = phoneW;
                    newPhoneH = (int)(phoneW / ratio);
                }
                else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM)
                {
                    newPhoneH = phoneH;
                    newPhoneW = (int)(phoneH * ratio);
                }
                else
                {
                    newPhoneW = phoneW;
                    newPhoneH = (int)(phoneW / ratio);
                }

                if (newPhoneW < MIN_PHONE_W) { newPhoneW = MIN_PHONE_W; newPhoneH = (int)(MIN_PHONE_W / ratio); }

                int newTotalW = newPhoneW;
                int newTotalH = newPhoneH + BUBBLE_H + BUBBLE_GAP;

                switch (edge)
                {
                    case WMSZ_RIGHT: case WMSZ_BOTTOM: case WMSZ_BOTTOMRIGHT:
                        r.Right = r.Left + newTotalW; r.Bottom = r.Top + newTotalH; break;
                    case WMSZ_LEFT: case WMSZ_BOTTOMLEFT:
                        r.Left = r.Right - newTotalW; r.Bottom = r.Top + newTotalH; break;
                    case WMSZ_TOP: case WMSZ_TOPRIGHT:
                        r.Right = r.Left + newTotalW; r.Top = r.Bottom - newTotalH; break;
                    case WMSZ_TOPLEFT:
                        r.Left = r.Right - newTotalW; r.Top = r.Bottom - newTotalH; break;
                }

                Marshal.StructureToPtr(r, m.LParam, true);
                m.Result = (IntPtr)1;
                return;
            }

            if (m.Msg == WM_NCHITTEST)
            {
                Point screenPoint = new Point(m.LParam.ToInt32() & 0xffff, m.LParam.ToInt32() >> 16);
                Point pt = this.PointToClient(screenPoint);

                // Drag handle = HTCAPTION (window move)
                if (rectDrag.Contains(pt)) { m.Result = (IntPtr)HTCAPTION; return; }

                // All other bubble buttons = HTCLIENT (handled via OnMouseDown)
                if (rectBubble.Contains(pt)) { m.Result = (IntPtr)HTCLIENT; return; }

                // Resize borders on the phone edges
                int border = 8;
                Rectangle rp = rectPhone;
                bool left   = pt.X >= rp.Left && pt.X <= rp.Left + border;
                bool right  = pt.X >= rp.Right - border && pt.X <= rp.Right;
                bool top    = pt.Y >= rp.Top && pt.Y <= rp.Top + border;
                bool bottom = pt.Y >= rp.Bottom - border && pt.Y <= rp.Bottom;

                if (top && left)     { m.Result = (IntPtr)HTTOPLEFT; return; }
                if (top && right)    { m.Result = (IntPtr)HTTOPRIGHT; return; }
                if (bottom && left)  { m.Result = (IntPtr)HTBOTTOMLEFT; return; }
                if (bottom && right) { m.Result = (IntPtr)HTBOTTOMRIGHT; return; }
                if (left)            { m.Result = (IntPtr)HTLEFT; return; }
                if (right)           { m.Result = (IntPtr)HTRIGHT; return; }
                if (top)             { m.Result = (IntPtr)HTTOP; return; }
                if (bottom)          { m.Result = (IntPtr)HTBOTTOM; return; }

                m.Result = (IntPtr)HTCLIENT;
                return;
            }

            base.WndProc(ref m);
        }

        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();

        [STAThread]
        public static void Main()
        {
            if (Environment.OSVersion.Version.Major >= 6) SetProcessDPIAware();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new CustomWindow());
        }
    }
}
