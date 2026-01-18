#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "WallRenderer.h"
#include "DimensionRenderer.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <cmath>

namespace winrt::estimate1
{
    // ============================================================================
    // PDF Export Settings
    // ============================================================================

    enum class PaperSize
    {
        A4,     // 210 ? 297 mm
        A3,     // 297 ? 420 mm
        A2,     // 420 ? 594 mm
        A1,     // 594 ? 841 mm
        A0      // 841 ? 1189 mm
    };

    enum class PaperOrientation
    {
        Portrait,
        Landscape
    };

    struct PdfExportSettings
    {
        // Масштаб чертежа
        double Scale{ 100.0 };  // 1:100 по умолчанию

        // Размер бумаги
        PaperSize Paper{ PaperSize::A4 };
        PaperOrientation Orientation{ PaperOrientation::Landscape };

        // Поля (мм)
        double MarginLeft{ 20.0 };
        double MarginRight{ 10.0 };
        double MarginTop{ 10.0 };
        double MarginBottom{ 10.0 };

        // Штамп (основная надпись)
        bool ShowTitleBlock{ true };
        std::wstring ProjectName;
        std::wstring DrawingTitle{ L"План этажа" };
        std::wstring Author;
        std::wstring SheetNumber{ L"1" };
        std::wstring TotalSheets{ L"1" };

        // Что показывать
        bool ShowWalls{ true };
        bool ShowDimensions{ true };
        bool ShowGrid{ false };
        bool ShowDxfReference{ false };
        bool ShowIfcReference{ false };

        // Цвета линий для PDF (чёрно-белый режим)
        bool BlackAndWhite{ true };
    };

    // ============================================================================
    // PDF Writer — генератор PDF файлов
    // ============================================================================

    class PdfWriter
    {
    public:
        PdfWriter() = default;

        bool Create(const std::wstring& filePath, double widthMm, double heightMm)
        {
            m_file.open(filePath, std::ios::binary);
            if (!m_file.is_open()) return false;

            m_widthPt = MmToPoints(widthMm);
            m_heightPt = MmToPoints(heightMm);
            m_objectOffsets.clear();
            m_currentObjectId = 0;
            m_contentStream.str("");
            m_contentStream.clear();

            // Заголовок PDF
            m_file << "%PDF-1.4\n";
            // Бинарный маркер для корректной обработки
            m_file << "%\xE2\xE3\xCF\xD3\n";

            return true;
        }

        // Примитивы рисования

        void SetLineWidth(double widthMm)
        {
            m_contentStream << std::fixed << std::setprecision(2)
                           << MmToPoints(widthMm) << " w\n";
        }

        void SetStrokeColor(int r, int g, int b)
        {
            m_contentStream << std::fixed << std::setprecision(3)
                           << (r / 255.0) << " " << (g / 255.0) << " " << (b / 255.0) << " RG\n";
        }

        void SetFillColor(int r, int g, int b)
        {
            m_contentStream << std::fixed << std::setprecision(3)
                           << (r / 255.0) << " " << (g / 255.0) << " " << (b / 255.0) << " rg\n";
        }

        void SetDashPattern(const std::vector<double>& pattern, double phase = 0)
        {
            m_contentStream << "[";
            for (size_t i = 0; i < pattern.size(); ++i)
            {
                if (i > 0) m_contentStream << " ";
                m_contentStream << std::fixed << std::setprecision(2) << MmToPoints(pattern[i]);
            }
            m_contentStream << "] " << phase << " d\n";
        }

        void SetSolidLine()
        {
            m_contentStream << "[] 0 d\n";
        }

        void MoveTo(double xMm, double yMm)
        {
            m_contentStream << std::fixed << std::setprecision(2)
                           << MmToPoints(xMm) << " " << MmToPoints(yMm) << " m\n";
        }

        void LineTo(double xMm, double yMm)
        {
            m_contentStream << std::fixed << std::setprecision(2)
                           << MmToPoints(xMm) << " " << MmToPoints(yMm) << " l\n";
        }

        void Rectangle(double xMm, double yMm, double wMm, double hMm)
        {
            m_contentStream << std::fixed << std::setprecision(2)
                           << MmToPoints(xMm) << " " << MmToPoints(yMm) << " "
                           << MmToPoints(wMm) << " " << MmToPoints(hMm) << " re\n";
        }

        void Stroke()
        {
            m_contentStream << "S\n";
        }

        void Fill()
        {
            m_contentStream << "f\n";
        }

        void FillAndStroke()
        {
            m_contentStream << "B\n";
        }

        void ClosePath()
        {
            m_contentStream << "h\n";
        }

        void CloseAndStroke()
        {
            m_contentStream << "s\n";
        }

        // Текст
        void BeginText()
        {
            m_contentStream << "BT\n";
        }

        void EndText()
        {
            m_contentStream << "ET\n";
        }

        void SetFont(const std::string& fontName, double sizePt)
        {
            m_contentStream << "/" << fontName << " " << sizePt << " Tf\n";
            m_currentFontSize = sizePt;
        }

        void SetTextPosition(double xMm, double yMm)
        {
            m_contentStream << std::fixed << std::setprecision(2)
                           << MmToPoints(xMm) << " " << MmToPoints(yMm) << " Td\n";
        }

        void ShowText(const std::string& text)
        {
            m_contentStream << "(";
            for (char c : text)
            {
                if (c == '(' || c == ')' || c == '\\')
                    m_contentStream << '\\';
                m_contentStream << c;
            }
            m_contentStream << ") Tj\n";
        }

        // Сохранение/восстановление состояния
        void SaveState()
        {
            m_contentStream << "q\n";
        }

        void RestoreState()
        {
            m_contentStream << "Q\n";
        }

        // Трансформации
        void Translate(double xMm, double yMm)
        {
            m_contentStream << "1 0 0 1 "
                           << std::fixed << std::setprecision(2)
                           << MmToPoints(xMm) << " " << MmToPoints(yMm) << " cm\n";
        }

        void Rotate(double angleDegrees)
        {
            double rad = angleDegrees * 3.14159265358979 / 180.0;
            double c = std::cos(rad);
            double s = std::sin(rad);
            m_contentStream << std::fixed << std::setprecision(4)
                           << c << " " << s << " " << (-s) << " " << c << " 0 0 cm\n";
        }

        bool Finalize()
        {
            // Записываем объекты PDF

            // 1. Catalog
            size_t catalogOffset = WriteObject(1, [&]() {
                m_file << "<< /Type /Catalog /Pages 2 0 R >>\n";
            });
            m_objectOffsets.push_back(catalogOffset);

            // 2. Pages
            size_t pagesOffset = WriteObject(2, [&]() {
                m_file << "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n";
            });
            m_objectOffsets.push_back(pagesOffset);

            // 3. Page
            size_t pageOffset = WriteObject(3, [&]() {
                m_file << "<< /Type /Page /Parent 2 0 R\n"
                       << "   /MediaBox [0 0 " << m_widthPt << " " << m_heightPt << "]\n"
                       << "   /Contents 4 0 R\n"
                       << "   /Resources << /Font << /F1 5 0 R >> >>\n"
                       << ">>\n";
            });
            m_objectOffsets.push_back(pageOffset);

            // 4. Content stream
            std::string content = m_contentStream.str();
            size_t contentOffset = WriteObject(4, [&]() {
                m_file << "<< /Length " << content.size() << " >>\n"
                       << "stream\n"
                       << content
                       << "endstream\n";
            });
            m_objectOffsets.push_back(contentOffset);

            // 5. Font (Helvetica - встроенный)
            size_t fontOffset = WriteObject(5, [&]() {
                m_file << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\n";
            });
            m_objectOffsets.push_back(fontOffset);

            // xref table
            size_t xrefOffset = m_file.tellp();
            m_file << "xref\n";
            m_file << "0 6\n";
            m_file << "0000000000 65535 f \n";
            for (size_t i = 0; i < 5; ++i)
            {
                m_file << std::setw(10) << std::setfill('0') << m_objectOffsets[i] << " 00000 n \n";
            }

            // trailer
            m_file << "trailer\n"
                   << "<< /Size 6 /Root 1 0 R >>\n"
                   << "startxref\n"
                   << xrefOffset << "\n"
                   << "%%EOF\n";

            m_file.close();
            return true;
        }

        // Утилиты
        static double MmToPoints(double mm) { return mm * 72.0 / 25.4; }
        static double PointsToMm(double pt) { return pt * 25.4 / 72.0; }

        static void GetPaperDimensions(PaperSize size, PaperOrientation orientation,
                                       double& widthMm, double& heightMm)
        {
            switch (size)
            {
            case PaperSize::A4: widthMm = 210; heightMm = 297; break;
            case PaperSize::A3: widthMm = 297; heightMm = 420; break;
            case PaperSize::A2: widthMm = 420; heightMm = 594; break;
            case PaperSize::A1: widthMm = 594; heightMm = 841; break;
            case PaperSize::A0: widthMm = 841; heightMm = 1189; break;
            }

            if (orientation == PaperOrientation::Landscape)
            {
                std::swap(widthMm, heightMm);
            }
        }

    private:
        std::ofstream m_file;
        double m_widthPt{ 0 };
        double m_heightPt{ 0 };
        std::vector<size_t> m_objectOffsets;
        int m_currentObjectId{ 0 };
        std::ostringstream m_contentStream;
        double m_currentFontSize{ 12 };

        template<typename Func>
        size_t WriteObject(int id, Func writeContent)
        {
            size_t offset = m_file.tellp();
            m_file << id << " 0 obj\n";
            writeContent();
            m_file << "endobj\n";
            return offset;
        }
    };

    // ============================================================================
    // Plan PDF Exporter
    // ============================================================================

    class PlanPdfExporter
    {
    public:
        bool Export(
            const DocumentModel& document,
            const std::vector<Dimension>& dimensions,
            const PdfExportSettings& settings,
            const std::wstring& filePath)
        {
            // Получаем размеры бумаги
            double paperWidthMm, paperHeightMm;
            PdfWriter::GetPaperDimensions(settings.Paper, settings.Orientation,
                                          paperWidthMm, paperHeightMm);

            // Создаём PDF
            PdfWriter pdf;
            if (!pdf.Create(filePath, paperWidthMm, paperHeightMm))
                return false;

            // Вычисляем область рисования (без полей и штампа)
            double drawAreaLeft = settings.MarginLeft;
            double drawAreaBottom = settings.MarginBottom;
            double drawAreaWidth = paperWidthMm - settings.MarginLeft - settings.MarginRight;
            double drawAreaHeight = paperHeightMm - settings.MarginTop - settings.MarginBottom;

            // Учитываем штамп (55 мм снизу справа)
            double titleBlockHeight = settings.ShowTitleBlock ? 55.0 : 0;
            double titleBlockWidth = settings.ShowTitleBlock ? 185.0 : 0;
            
            if (settings.ShowTitleBlock)
            {
                drawAreaHeight -= titleBlockHeight;
            }

            // Вычисляем границы модели
            WorldPoint modelMin, modelMax;
            CalculateModelBounds(document, modelMin, modelMax);

            // Масштаб: 1:Scale означает 1 мм на бумаге = Scale мм в модели
            double scaleFactor = 1.0 / settings.Scale;

            // Центрируем модель в области рисования
            double modelWidthMm = (modelMax.X - modelMin.X) * scaleFactor;
            double modelHeightMm = (modelMax.Y - modelMin.Y) * scaleFactor;

            double offsetX = drawAreaLeft + (drawAreaWidth - modelWidthMm) / 2.0;
            double offsetY = drawAreaBottom + titleBlockHeight + (drawAreaHeight - modelHeightMm) / 2.0;

            // Лямбда для преобразования координат модели в координаты PDF
            auto modelToPdf = [&](const WorldPoint& pt) -> std::pair<double, double> {
                double x = offsetX + (pt.X - modelMin.X) * scaleFactor;
                double y = offsetY + (pt.Y - modelMin.Y) * scaleFactor;
                // Инвертируем Y для PDF (Y вверх)
                y = paperHeightMm - y;
                return { x, y };
            };

            // Рисуем рамку чертежа
            pdf.SetStrokeColor(0, 0, 0);
            pdf.SetLineWidth(0.5);
            pdf.Rectangle(settings.MarginLeft, settings.MarginBottom,
                         paperWidthMm - settings.MarginLeft - settings.MarginRight,
                         paperHeightMm - settings.MarginTop - settings.MarginBottom);
            pdf.Stroke();

            // Рисуем стены
            if (settings.ShowWalls)
            {
                for (const auto& wall : document.GetWalls())
                {
                    if (!wall) continue;

                    // Настраиваем стиль линии по WorkState
                    pdf.SetLineWidth(0.35);
                    
                    switch (wall->GetWorkState())
                    {
                    case WorkStateNative::Existing:
                        pdf.SetStrokeColor(0, 0, 0);
                        pdf.SetFillColor(200, 200, 200);
                        pdf.SetSolidLine();
                        break;
                    case WorkStateNative::Demolish:
                        if (settings.BlackAndWhite)
                        {
                            pdf.SetStrokeColor(0, 0, 0);
                            pdf.SetFillColor(255, 255, 255);
                        }
                        else
                        {
                            pdf.SetStrokeColor(200, 0, 0);
                            pdf.SetFillColor(255, 200, 200);
                        }
                        pdf.SetDashPattern({ 2.0, 1.0 });
                        break;
                    case WorkStateNative::New:
                        if (settings.BlackAndWhite)
                        {
                            pdf.SetStrokeColor(0, 0, 0);
                            pdf.SetFillColor(230, 230, 230);
                        }
                        else
                        {
                            pdf.SetStrokeColor(0, 0, 180);
                            pdf.SetFillColor(200, 200, 255);
                        }
                        pdf.SetSolidLine();
                        break;
                    }

                    // Получаем угловые точки стены
                    WorldPoint p1, p2, p3, p4;
                    wall->GetCornerPoints(p1, p2, p3, p4);

                    auto [x1, y1] = modelToPdf(p1);
                    auto [x2, y2] = modelToPdf(p2);
                    auto [x3, y3] = modelToPdf(p3);
                    auto [x4, y4] = modelToPdf(p4);

                    // Рисуем контур стены
                    pdf.MoveTo(x1, y1);
                    pdf.LineTo(x2, y2);
                    pdf.LineTo(x3, y3);
                    pdf.LineTo(x4, y4);
                    pdf.ClosePath();
                    pdf.FillAndStroke();

                    pdf.SetSolidLine();
                }
            }

            // Рисуем размеры
            if (settings.ShowDimensions)
            {
                pdf.SetStrokeColor(0, 0, 0);
                pdf.SetFillColor(0, 0, 0);
                pdf.SetLineWidth(0.18);
                pdf.SetSolidLine();

                for (const auto& dim : dimensions)
                {
                    WorldPoint p1 = dim.GetP1();
                    WorldPoint p2 = dim.GetP2();
                    double offset = dim.GetOffset();
                    double value = dim.GetValueMm();

                    // Вычисляем точки размера
                    WorldPoint dir{
                        p2.X - p1.X,
                        p2.Y - p1.Y
                    };
                    double len = std::sqrt(dir.X * dir.X + dir.Y * dir.Y);
                    if (len < 0.001) continue;
                    dir.X /= len;
                    dir.Y /= len;

                    WorldPoint perp{ -dir.Y, dir.X };

                    WorldPoint dimP1{
                        p1.X + perp.X * offset,
                        p1.Y + perp.Y * offset
                    };
                    WorldPoint dimP2{
                        p2.X + perp.X * offset,
                        p2.Y + perp.Y * offset
                    };

                    auto pdfDimP1 = modelToPdf(dimP1);
                    auto pdfDimP2 = modelToPdf(dimP2);
                    auto pdfP1 = modelToPdf(p1);
                    auto pdfP2 = modelToPdf(p2);

                    double dx1 = pdfDimP1.first;
                    double dy1 = pdfDimP1.second;
                    double dx2 = pdfDimP2.first;
                    double dy2 = pdfDimP2.second;
                    double ex1 = pdfP1.first;
                    double ey1 = pdfP1.second;
                    double ex2 = pdfP2.first;
                    double ey2 = pdfP2.second;

                    // Выносные линии
                    pdf.MoveTo(ex1, ey1);
                    pdf.LineTo(dx1, dy1);
                    pdf.Stroke();

                    pdf.MoveTo(ex2, ey2);
                    pdf.LineTo(dx2, dy2);
                    pdf.Stroke();

                    // Размерная линия
                    pdf.MoveTo(dx1, dy1);
                    pdf.LineTo(dx2, dy2);
                    pdf.Stroke();

                    // Засечки
                    double tickLen = 1.5 * scaleFactor;
                    pdf.MoveTo(dx1 - tickLen, dy1 - tickLen);
                    pdf.LineTo(dx1 + tickLen, dy1 + tickLen);
                    pdf.Stroke();

                    pdf.MoveTo(dx2 - tickLen, dy2 - tickLen);
                    pdf.LineTo(dx2 + tickLen, dy2 + tickLen);
                    pdf.Stroke();

                    // Текст размера
                    std::ostringstream textStream;
                    if (value >= 1000)
                        textStream << std::fixed << std::setprecision(2) << (value / 1000.0);
                    else
                        textStream << static_cast<int>(value);

                    double textX = (dx1 + dx2) / 2.0;
                    double textY = (dy1 + dy2) / 2.0 + 1.5;

                    pdf.BeginText();
                    pdf.SetFont("F1", 8);
                    pdf.SetTextPosition(textX - 3, textY);
                    pdf.ShowText(textStream.str());
                    pdf.EndText();
                }
            }

            // Рисуем штамп
            if (settings.ShowTitleBlock)
            {
                DrawTitleBlock(pdf, settings, paperWidthMm, paperHeightMm);
            }

            return pdf.Finalize();
        }

    private:
        void CalculateModelBounds(
            const DocumentModel& document,
            WorldPoint& minPt,
            WorldPoint& maxPt)
        {
            minPt = { 1e10, 1e10 };
            maxPt = { -1e10, -1e10 };

            for (const auto& wall : document.GetWalls())
            {
                if (!wall) continue;

                WorldPoint p1, p2, p3, p4;
                wall->GetCornerPoints(p1, p2, p3, p4);

                minPt.X = (std::min)({ minPt.X, p1.X, p2.X, p3.X, p4.X });
                minPt.Y = (std::min)({ minPt.Y, p1.Y, p2.Y, p3.Y, p4.Y });
                maxPt.X = (std::max)({ maxPt.X, p1.X, p2.X, p3.X, p4.X });
                maxPt.Y = (std::max)({ maxPt.Y, p1.Y, p2.Y, p3.Y, p4.Y });
            }

            // Добавляем отступ
            double padding = 500; // 500 мм
            minPt.X -= padding;
            minPt.Y -= padding;
            maxPt.X += padding;
            maxPt.Y += padding;

            // Защита от пустой модели
            if (maxPt.X <= minPt.X || maxPt.Y <= minPt.Y)
            {
                minPt = { -5000, -5000 };
                maxPt = { 5000, 5000 };
            }
        }

        void DrawTitleBlock(
            PdfWriter& pdf,
            const PdfExportSettings& settings,
            double paperWidthMm,
            double paperHeightMm)
        {
            // Штамп 185?55 мм в правом нижнем углу
            double blockWidth = 185;
            double blockHeight = 55;
            double blockX = paperWidthMm - settings.MarginRight - blockWidth;
            double blockY = settings.MarginBottom;

            pdf.SetStrokeColor(0, 0, 0);
            pdf.SetLineWidth(0.5);

            // Внешняя рамка
            pdf.Rectangle(blockX, blockY, blockWidth, blockHeight);
            pdf.Stroke();

            // Внутренние линии
            pdf.SetLineWidth(0.25);

            // Горизонтальные линии
            double rowHeight = blockHeight / 5.0;
            for (int i = 1; i < 5; ++i)
            {
                double y = blockY + i * rowHeight;
                pdf.MoveTo(blockX, y);
                pdf.LineTo(blockX + blockWidth, y);
                pdf.Stroke();
            }

            // Вертикальные линии
            double col1 = 70;  // Ширина первого столбца
            double col2 = 50;  // Ширина второго столбца
            
            // Линия после первого столбца
            pdf.MoveTo(blockX + col1, blockY);
            pdf.LineTo(blockX + col1, blockY + blockHeight);
            pdf.Stroke();

            // Линия после второго столбца
            pdf.MoveTo(blockX + col1 + col2, blockY);
            pdf.LineTo(blockX + col1 + col2, blockY + blockHeight);
            pdf.Stroke();

            // Текст
            pdf.SetFillColor(0, 0, 0);

            // Название проекта (крупно, сверху)
            pdf.BeginText();
            pdf.SetFont("F1", 10);
            pdf.SetTextPosition(blockX + 5, blockY + blockHeight - rowHeight + 3);
            std::string projectName = WStringToAscii(settings.ProjectName.empty() ? L"Проект" : settings.ProjectName);
            pdf.ShowText(projectName);
            pdf.EndText();

            // Название чертежа
            pdf.BeginText();
            pdf.SetFont("F1", 8);
            pdf.SetTextPosition(blockX + 5, blockY + blockHeight - 2 * rowHeight + 3);
            pdf.ShowText(WStringToAscii(settings.DrawingTitle));
            pdf.EndText();

            // Масштаб
            pdf.BeginText();
            pdf.SetFont("F1", 8);
            pdf.SetTextPosition(blockX + col1 + 5, blockY + blockHeight - 2 * rowHeight + 3);
            std::ostringstream scaleStr;
            scaleStr << "1:" << static_cast<int>(settings.Scale);
            pdf.ShowText(scaleStr.str());
            pdf.EndText();

            // Дата
            pdf.BeginText();
            pdf.SetFont("F1", 7);
            pdf.SetTextPosition(blockX + col1 + col2 + 5, blockY + blockHeight - 2 * rowHeight + 3);
            
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);
            std::ostringstream dateStr;
            dateStr << std::setfill('0') << std::setw(2) << tm.tm_mday << "."
                   << std::setw(2) << (tm.tm_mon + 1) << "."
                   << (tm.tm_year + 1900);
            pdf.ShowText(dateStr.str());
            pdf.EndText();

            // Автор
            if (!settings.Author.empty())
            {
                pdf.BeginText();
                pdf.SetFont("F1", 7);
                pdf.SetTextPosition(blockX + 5, blockY + rowHeight + 3);
                pdf.ShowText(WStringToAscii(settings.Author));
                pdf.EndText();
            }

            // Номер листа
            pdf.BeginText();
            pdf.SetFont("F1", 8);
            pdf.SetTextPosition(blockX + col1 + col2 + 5, blockY + 3);
            std::string sheetNum = WStringToAscii(settings.SheetNumber) + "/" + WStringToAscii(settings.TotalSheets);
            pdf.ShowText(sheetNum);
            pdf.EndText();

            // Подписи полей
            pdf.BeginText();
            pdf.SetFont("F1", 6);
            pdf.SetTextPosition(blockX + col1 + 2, blockY + blockHeight - rowHeight + 3);
            pdf.ShowText("Masshtab");
            pdf.EndText();

            pdf.BeginText();
            pdf.SetFont("F1", 6);
            pdf.SetTextPosition(blockX + col1 + col2 + 2, blockY + blockHeight - rowHeight + 3);
            pdf.ShowText("Data");
            pdf.EndText();

            pdf.BeginText();
            pdf.SetFont("F1", 6);
            pdf.SetTextPosition(blockX + col1 + col2 + 2, blockY + rowHeight + 3);
            pdf.ShowText("List");
            pdf.EndText();
        }

        std::string WStringToAscii(const std::wstring& wstr)
        {
            // Простое преобразование - заменяем не-ASCII символы на ?
            std::string result;
            for (wchar_t c : wstr)
            {
                if (c < 128)
                    result += static_cast<char>(c);
                else
                    result += '?';  // PDF Type1 шрифты не поддерживают кириллицу
            }
            return result;
        }
    };

} // namespace winrt::estimate1
