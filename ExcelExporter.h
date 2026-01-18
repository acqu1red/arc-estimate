#pragma once

#include "pch.h"
#include "EstimationEngine.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <filesystem>
#include <cwctype>

namespace winrt::estimate1
{
    // ============================================================================
    // Excel Cell — ячейка Excel
    // ============================================================================

    struct ExcelCell
    {
        std::wstring Value;
        bool IsNumber{ false };
        bool IsBold{ false };
        bool IsHeader{ false };
        int ColSpan{ 1 };
    };

    // ============================================================================
    // Excel Row — строка Excel
    // ============================================================================

    struct ExcelRow
    {
        std::vector<ExcelCell> Cells;
        bool IsHeaderRow{ false };
        bool IsTotalRow{ false };
    };

    // ============================================================================
    // Excel Sheet — лист Excel
    // ============================================================================

    struct ExcelSheet
    {
        std::wstring Name{ L"Смета" };
        std::vector<ExcelRow> Rows;
        std::vector<double> ColumnWidths;
    };

    // ============================================================================
    // CSV Exporter — экспорт в CSV (простой формат)
    // ============================================================================

    class CsvExporter
    {
    public:
        static bool Export(
            const EstimationResult& result,
            const std::wstring& filePath)
        {
            std::wofstream file(filePath);
            if (!file.is_open()) return false;

            // BOM для UTF-8
            file << L"\xFEFF";

            // Заголовок
            file << L"СМЕТА НА РЕМОНТНЫЕ РАБОТЫ\n";
            file << L"Дата:;" << result.CalculationDate << L"\n";
            file << L"\n";

            // Заголовки столбцов
            file << L"№;Категория;Описание работ;Ед.изм.;Кол-во;Цена за ед.;Сумма\n";

            int rowNum = 1;
            for (const auto& item : result.Items)
            {
                // Пропускаем заголовки разделов
                if (item.Description.empty())
                {
                    file << L";" << item.Category << L";;;;;\n";
                    continue;
                }

                file << rowNum++ << L";"
                     << item.Category << L";"
                     << item.Description << L";"
                     << item.Unit << L";"
                     << FormatNumber(item.Quantity) << L";"
                     << FormatNumber(item.UnitPrice) << L";"
                     << FormatNumber(item.TotalPrice) << L"\n";
            }

            // Итоги
            file << L"\n";
            file << L";;;;;Итого демонтаж:;" << FormatNumber(result.DemolitionSubtotal) << L"\n";
            file << L";;;;;Итого строительство:;" << FormatNumber(result.ConstructionSubtotal) << L"\n";
            file << L";;;;;Подитог:;" << FormatNumber(result.Subtotal) << L"\n";
            file << L";;;;;Непредвиденные (10%):;" << FormatNumber(result.Contingency) << L"\n";
            file << L";;;;;ВСЕГО:;" << FormatNumber(result.GrandTotal) << L"\n";

            // R5.5: Экспорт сводки по зонам
            if (!result.ZoneSummaries.empty())
            {
                file << L"\n\nСВОДКА ПО ЗОНАМ\n";
                file << L"Зона;Помещений;Площадь (м²);Периметр (м);Площадь стен (м²);Отделка пола;Отделка потолка;Отделка стен;Итого отделка\n";
                
                double totalFinish = 0.0;
                for (const auto& zone : result.ZoneSummaries)
                {
                    file << zone.ZoneName << L";"
                         << zone.RoomCount << L";"
                         << FormatNumber(zone.TotalAreaSqM) << L";"
                         << FormatNumber(zone.TotalPerimeterM) << L";"
                         << FormatNumber(zone.TotalWallAreaSqM) << L";"
                         << FormatNumber(zone.FloorFinishCost) << L";"
                         << FormatNumber(zone.CeilingFinishCost) << L";"
                         << FormatNumber(zone.WallFinishCost) << L";"
                         << FormatNumber(zone.TotalFinishCost) << L"\n";
                    totalFinish += zone.TotalFinishCost;
                }
                
                file << L"ИТОГО;;;;;;;;;" << FormatNumber(totalFinish) << L"\n";
            }

            file.close();
            return true;
        }

    private:
        static std::wstring FormatNumber(double value)
        {
            wchar_t buffer[64];
            swprintf_s(buffer, L"%.2f", value);
            return buffer;
        }
    };

    // ============================================================================
    // XLSX Writer — создание файлов Excel (XLSX)
    // ============================================================================

    class XlsxWriter
    {
    public:
        XlsxWriter() = default;

        bool Create(const std::wstring& filePath)
        {
            m_filePath = filePath;
            m_sheets.clear();
            return true;
        }

        ExcelSheet& AddSheet(const std::wstring& name)
        {
            ExcelSheet sheet;
            sheet.Name = name;
            sheet.ColumnWidths = { 5, 20, 40, 10, 12, 15, 18 }; // 7 столбцов
            m_sheets.push_back(sheet);
            return m_sheets.back();
        }

        void AddRow(ExcelSheet& sheet, const std::vector<std::wstring>& values,
                   bool isHeader = false, bool isTotal = false)
        {
            ExcelRow row;
            row.IsHeaderRow = isHeader;
            row.IsTotalRow = isTotal;

            for (const auto& val : values)
            {
                ExcelCell cell;
                cell.Value = val;
                cell.IsBold = isHeader || isTotal;
                cell.IsHeader = isHeader;
                
                // Определяем, является ли значение числом
                if (!val.empty())
                {
                    bool isNum = true;
                    bool hasDot = false;
                    for (wchar_t c : val)
                    {
                        if (c == L'.' || c == L',')
                        {
                            if (hasDot) { isNum = false; break; }
                            hasDot = true;
                        }
                        else if (!iswdigit(c) && c != L'-' && c != L' ')
                        {
                            isNum = false;
                            break;
                        }
                    }
                    cell.IsNumber = isNum && !val.empty();
                }
                
                row.Cells.push_back(cell);
            }

            sheet.Rows.push_back(row);
        }

        void AddEmptyRow(ExcelSheet& sheet)
        {
            sheet.Rows.push_back(ExcelRow{});
        }

        void AddSectionHeader(ExcelSheet& sheet, const std::wstring& title)
        {
            ExcelRow row;
            row.IsHeaderRow = true;
            
            ExcelCell cell;
            cell.Value = title;
            cell.IsBold = true;
            cell.IsHeader = true;
            cell.ColSpan = 7;
            row.Cells.push_back(cell);
            
            sheet.Rows.push_back(row);
        }

        bool Save()
        {
            // Создаём временную директорию для файлов XLSX
            std::filesystem::path tempDir = std::filesystem::temp_directory_path() / L"xlsx_temp";
            std::filesystem::create_directories(tempDir);

            try
            {
                // Создаём структуру XLSX
                CreateContentTypes(tempDir);
                CreateRels(tempDir);
                CreateWorkbook(tempDir);
                CreateWorkbookRels(tempDir);
                CreateStyles(tempDir);
                
                for (size_t i = 0; i < m_sheets.size(); ++i)
                {
                    CreateSheet(tempDir, i);
                }

                // Создаём ZIP архив (XLSX = ZIP)
                bool result = CreateZipArchive(tempDir, m_filePath);

                // Удаляем временные файлы
                std::filesystem::remove_all(tempDir);

                return result;
            }
            catch (...)
            {
                std::filesystem::remove_all(tempDir);
                return false;
            }
        }

    private:
        std::wstring m_filePath;
        std::vector<ExcelSheet> m_sheets;

        // Создание [Content_Types].xml
        void CreateContentTypes(const std::filesystem::path& dir)
        {
            std::ofstream file(dir / "[Content_Types].xml");
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n";
            file << "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n";
            file << "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n";
            file << "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n";
            file << "  <Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n";
            
            for (size_t i = 0; i < m_sheets.size(); ++i)
            {
                file << "  <Override PartName=\"/xl/worksheets/sheet" << (i + 1) 
                     << ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n";
            }
            
            file << "</Types>\n";
            file.close();
        }

        // Создание _rels/.rels
        void CreateRels(const std::filesystem::path& dir)
        {
            std::filesystem::create_directories(dir / "_rels");
            std::ofstream file(dir / "_rels" / ".rels");
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
            file << "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n";
            file << "</Relationships>\n";
            file.close();
        }

        // Создание xl/workbook.xml
        void CreateWorkbook(const std::filesystem::path& dir)
        {
            std::filesystem::create_directories(dir / "xl");
            std::ofstream file(dir / "xl" / "workbook.xml");
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n";
            file << "  <sheets>\n";
            
            for (size_t i = 0; i < m_sheets.size(); ++i)
            {
                std::string name = WStringToUtf8(m_sheets[i].Name);
                file << "    <sheet name=\"" << EscapeXml(name) << "\" sheetId=\"" << (i + 1) 
                     << "\" r:id=\"rId" << (i + 1) << "\"/>\n";
            }
            
            file << "  </sheets>\n";
            file << "</workbook>\n";
            file.close();
        }

        // Создание xl/_rels/workbook.xml.rels
        void CreateWorkbookRels(const std::filesystem::path& dir)
        {
            std::filesystem::create_directories(dir / "xl" / "_rels");
            std::ofstream file(dir / "xl" / "_rels" / "workbook.xml.rels");
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
            
            for (size_t i = 0; i < m_sheets.size(); ++i)
            {
                file << "  <Relationship Id=\"rId" << (i + 1) 
                     << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" 
                     << (i + 1) << ".xml\"/>\n";
            }
            
            file << "  <Relationship Id=\"rId" << (m_sheets.size() + 1) 
                 << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n";
            file << "</Relationships>\n";
            file.close();
        }

        // Создание xl/styles.xml
        void CreateStyles(const std::filesystem::path& dir)
        {
            std::ofstream file(dir / "xl" / "styles.xml");
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n";
            
            // Форматы чисел
            file << "  <numFmts count=\"2\">\n";
            file << "    <numFmt numFmtId=\"164\" formatCode=\"#,##0.00\"/>\n";
            file << "    <numFmt numFmtId=\"165\" formatCode=\"#,##0.00\\ \\?\"/>\n";
            file << "  </numFmts>\n";
            
            // Шрифты
            file << "  <fonts count=\"2\">\n";
            file << "    <font><sz val=\"11\"/><name val=\"Calibri\"/></font>\n";
            file << "    <font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font>\n";
            file << "  </fonts>\n";
            
            // Заливки
            file << "  <fills count=\"3\">\n";
            file << "    <fill><patternFill patternType=\"none\"/></fill>\n";
            file << "    <fill><patternFill patternType=\"gray125\"/></fill>\n";
            file << "    <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFE0E0E0\"/></patternFill></fill>\n";
            file << "  </fills>\n";
            
            // Границы
            file << "  <borders count=\"2\">\n";
            file << "    <border/>\n";
            file << "    <border><left style=\"thin\"/><right style=\"thin\"/><top style=\"thin\"/><bottom style=\"thin\"/></border>\n";
            file << "  </borders>\n";
            
            // Стили ячеек
            file << "  <cellXfs count=\"6\">\n";
            file << "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n";                          // 0 - обычный
            file << "    <xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n";  // 1 - заголовок
            file << "    <xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"1\" applyNumberFormat=\"1\" applyBorder=\"1\"/>\n";        // 2 - число с границей
            file << "    <xf numFmtId=\"165\" fontId=\"0\" fillId=\"0\" borderId=\"1\" applyNumberFormat=\"1\" applyBorder=\"1\"/>\n";        // 3 - валюта с границей
            file << "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" applyBorder=\"1\"/>\n";         // 4 - текст с границей
            file << "    <xf numFmtId=\"165\" fontId=\"1\" fillId=\"2\" borderId=\"1\" applyNumberFormat=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n";  // 5 - итого
            file << "  </cellXfs>\n";
            
            file << "</styleSheet>\n";
            file.close();
        }

        // Создание xl/worksheets/sheet{n}.xml
        void CreateSheet(const std::filesystem::path& dir, size_t sheetIndex)
        {
            std::filesystem::create_directories(dir / "xl" / "worksheets");
            std::ofstream file(dir / "xl" / "worksheets" / ("sheet" + std::to_string(sheetIndex + 1) + ".xml"));
            
            const auto& sheet = m_sheets[sheetIndex];
            
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            file << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n";
            
            // Ширина столбцов
            if (!sheet.ColumnWidths.empty())
            {
                file << "  <cols>\n";
                for (size_t i = 0; i < sheet.ColumnWidths.size(); ++i)
                {
                    file << "    <col min=\"" << (i + 1) << "\" max=\"" << (i + 1) 
                         << "\" width=\"" << sheet.ColumnWidths[i] << "\" customWidth=\"1\"/>\n";
                }
                file << "  </cols>\n";
            }
            
            // Данные
            file << "  <sheetData>\n";
            
            for (size_t rowIdx = 0; rowIdx < sheet.Rows.size(); ++rowIdx)
            {
                const auto& row = sheet.Rows[rowIdx];
                file << "    <row r=\"" << (rowIdx + 1) << "\">\n";
                
                size_t colIdx = 0;
                for (const auto& cell : row.Cells)
                {
                    std::string cellRef = GetCellRef(rowIdx, colIdx);
                    
                    int styleId = 0;
                    if (row.IsHeaderRow) styleId = 1;
                    else if (row.IsTotalRow) styleId = 5;
                    else if (cell.IsNumber) styleId = (cell.Value.find(L'?') != std::wstring::npos ? 3 : 2);
                    else styleId = 4;
                    
                    std::string value = WStringToUtf8(cell.Value);
                    
                    if (cell.IsNumber && !cell.Value.empty())
                    {
                        // Убираем пробелы и символ валюты для числа
                        std::wstring numStr = cell.Value;
                        numStr.erase(std::remove(numStr.begin(), numStr.end(), L' '), numStr.end());
                        numStr.erase(std::remove(numStr.begin(), numStr.end(), L'?'), numStr.end());
                        std::replace(numStr.begin(), numStr.end(), L',', L'.');
                        
                        file << "      <c r=\"" << cellRef << "\" s=\"" << styleId << "\">"
                             << "<v>" << WStringToUtf8(numStr) << "</v></c>\n";
                    }
                    else
                    {
                        file << "      <c r=\"" << cellRef << "\" s=\"" << styleId << "\" t=\"inlineStr\">"
                             << "<is><t>" << EscapeXml(value) << "</t></is></c>\n";
                    }
                    
                    colIdx++;
                }
                
                file << "    </row>\n";
            }
            
            file << "  </sheetData>\n";
            file << "</worksheet>\n";
            file.close();
        }

        // Получить ссылку на ячейку (A1, B2, ...)
        std::string GetCellRef(size_t row, size_t col)
        {
            std::string colStr;
            size_t c = col;
            do {
                colStr = char('A' + c % 26) + colStr;
                c = c / 26;
            } while (c > 0 && (c--, true));
            
            return colStr + std::to_string(row + 1);
        }

        // Конвертация wstring в UTF-8
        std::string WStringToUtf8(const std::wstring& wstr)
        {
            if (wstr.empty()) return "";
            
            int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string result(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
            return result;
        }

        // Экранирование XML
        std::string EscapeXml(const std::string& str)
        {
            std::string result;
            for (char c : str)
            {
                switch (c)
                {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c;
                }
            }
            return result;
        }

        // Создание ZIP архива (использует Windows API)
        bool CreateZipArchive(const std::filesystem::path& sourceDir, const std::wstring& zipPath)
        {
            // Используем PowerShell для создания ZIP
            std::wstring psCommand = L"Compress-Archive -Path \"" + sourceDir.wstring() + L"\\*\" -DestinationPath \"" + zipPath + L"\" -Force";
            
            // Переименовываем .zip в .xlsx
            std::wstring tempZip = zipPath + L".zip";
            psCommand = L"Compress-Archive -Path \"" + sourceDir.wstring() + L"\\*\" -DestinationPath \"" + tempZip + L"\" -Force";
            
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            
            std::wstring cmdLine = L"powershell.exe -NoProfile -Command \"" + psCommand + L"\"";
            
            if (CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, 
                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 30000);  // 30 секунд таймаут
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                // Переименовываем в .xlsx
                if (std::filesystem::exists(tempZip))
                {
                    std::filesystem::rename(tempZip, zipPath);
                    return true;
                }
            }
            
            return false;
        }
    };

    // ============================================================================
    // Excel Exporter — высокоуровневый экспорт сметы в Excel
    // ============================================================================

    class ExcelExporter
    {
    public:
        static bool Export(
            const EstimationResult& result,
            const std::wstring& filePath)
        {
            XlsxWriter writer;
            if (!writer.Create(filePath)) return false;

            ExcelSheet& sheet = writer.AddSheet(L"Смета");

            // Заголовок документа
            writer.AddRow(sheet, { L"СМЕТА НА РЕМОНТНЫЕ РАБОТЫ", L"", L"", L"", L"", L"", L"" }, true);
            writer.AddRow(sheet, { L"Дата:", result.CalculationDate, L"", L"", L"", L"", L"" });
            if (!result.ProjectName.empty())
            {
                writer.AddRow(sheet, { L"Проект:", result.ProjectName, L"", L"", L"", L"", L"" });
            }
            writer.AddEmptyRow(sheet);

            // Заголовки столбцов
            writer.AddRow(sheet, 
                { L"№", L"Категория", L"Описание работ", L"Ед.изм.", L"Кол-во", L"Цена", L"Сумма" }, 
                true);

            // Строки сметы
            int rowNum = 1;
            for (const auto& item : result.Items)
            {
                if (item.Description.empty())
                {
                    // Заголовок раздела
                    writer.AddEmptyRow(sheet);
                    writer.AddSectionHeader(sheet, item.Category);
                    continue;
                }

                wchar_t numBuf[16], qtyBuf[32], priceBuf[32], totalBuf[32];
                swprintf_s(numBuf, L"%d", rowNum++);
                swprintf_s(qtyBuf, L"%.2f", item.Quantity);
                swprintf_s(priceBuf, L"%.2f", item.UnitPrice);
                swprintf_s(totalBuf, L"%.2f", item.TotalPrice);

                writer.AddRow(sheet, 
                    { numBuf, item.Category, item.Description, item.Unit, qtyBuf, priceBuf, totalBuf });
            }

            // Пустая строка перед итогами
            writer.AddEmptyRow(sheet);

            // Итоги
            wchar_t buf[64];
            
            if (result.DemolitionSubtotal > 0)
            {
                swprintf_s(buf, L"%.2f", result.DemolitionSubtotal);
                writer.AddRow(sheet, { L"", L"", L"", L"", L"", L"Итого демонтаж:", buf }, false, true);
            }
            
            if (result.ConstructionSubtotal > 0)
            {
                swprintf_s(buf, L"%.2f", result.ConstructionSubtotal);
                writer.AddRow(sheet, { L"", L"", L"", L"", L"", L"Итого строительство:", buf }, false, true);
            }
            
            swprintf_s(buf, L"%.2f", result.Subtotal);
            writer.AddRow(sheet, { L"", L"", L"", L"", L"", L"Подитог:", buf }, false, true);
            
            swprintf_s(buf, L"%.2f", result.Contingency);
            writer.AddRow(sheet, { L"", L"", L"", L"", L"", L"Непредвиденные (10%):", buf }, false, true);
            
            swprintf_s(buf, L"%.2f", result.GrandTotal);
            writer.AddRow(sheet, { L"", L"", L"", L"", L"", L"ВСЕГО:", buf }, false, true);

            return writer.Save();
        }
    };

} // namespace winrt::estimate1
