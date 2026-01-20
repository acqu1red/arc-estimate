#pragma once

// ============================================================================
// ARC-Estimate Unit Tests
// ������� �������� ��������� ��� ������� ������������
// ============================================================================

#include "pch.h"
#include "Camera.h"
#include "Models.h"
#include "Element.h"
#include "WallType.h"
#include "Material.h"
#include "Dimension.h"
#include "EstimationEngine.h"
#include "DrawingTools.h"
#include <vector>
#include <string>
#include <functional>
#include <sstream>
#include <cmath>

namespace winrt::estimate1::tests
{
    // ============================================================================
    // Test Framework
    // ============================================================================

    struct TestResult
    {
        std::wstring TestName;
        bool Passed{ false };
        std::wstring Message;
        double DurationMs{ 0.0 };
    };

    struct TestSuite
    {
        std::wstring Name;
        std::vector<TestResult> Results;
        int PassedCount{ 0 };
        int FailedCount{ 0 };
        double TotalDurationMs{ 0.0 };
    };

    class TestRunner
    {
    public:
        using TestFunc = std::function<void()>;

        void AddTest(const std::wstring& name, TestFunc func)
        {
            m_tests.push_back({ name, func });
        }

        TestSuite Run(const std::wstring& suiteName)
        {
            TestSuite suite;
            suite.Name = suiteName;

            for (const auto& [name, func] : m_tests)
            {
                TestResult result;
                result.TestName = name;

                auto start = std::chrono::high_resolution_clock::now();

                try
                {
                    func();
                    result.Passed = true;
                    result.Message = L"OK";
                    suite.PassedCount++;
                }
                catch (const std::exception& e)
                {
                    result.Passed = false;
                    // Convert exception message
                    std::string msg = e.what();
                    result.Message = std::wstring(msg.begin(), msg.end());
                    suite.FailedCount++;
                }
                catch (...)
                {
                    result.Passed = false;
                    result.Message = L"Unknown exception";
                    suite.FailedCount++;
                }

                auto end = std::chrono::high_resolution_clock::now();
                result.DurationMs = std::chrono::duration<double, std::milli>(end - start).count();
                suite.TotalDurationMs += result.DurationMs;

                suite.Results.push_back(result);
            }

            return suite;
        }

        void Clear() { m_tests.clear(); }

    private:
        std::vector<std::pair<std::wstring, TestFunc>> m_tests;
    };

    // Assertion helpers
    class AssertionError : public std::runtime_error
    {
    public:
        explicit AssertionError(const std::string& msg) : std::runtime_error(msg) {}
    };

    inline void Assert(bool condition, const char* message = "Assertion failed")
    {
        if (!condition) throw AssertionError(message);
    }

    inline void AssertEqual(double a, double b, double tolerance = 0.001, const char* message = "Values not equal")
    {
        if (std::abs(a - b) > tolerance) throw AssertionError(message);
    }

    inline void AssertEqual(int a, int b, const char* message = "Values not equal")
    {
        if (a != b) throw AssertionError(message);
    }

    inline void AssertTrue(bool value, const char* message = "Expected true")
    {
        if (!value) throw AssertionError(message);
    }

    inline void AssertFalse(bool value, const char* message = "Expected false")
    {
        if (value) throw AssertionError(message);
    }

    // ============================================================================
    // Camera Tests
    // ============================================================================

    inline TestSuite RunCameraTests()
    {
        TestRunner runner;

        runner.AddTest(L"Camera_DefaultState", []() {
            Camera camera;
            AssertEqual(camera.GetZoom(), 1.0, 0.001, "Default zoom should be 1.0");
        });

        runner.AddTest(L"Camera_WorldToScreen_Origin", []() {
            Camera camera;
            camera.SetCanvasSize(800, 600);
            
            WorldPoint world{ 0, 0 };
            ScreenPoint screen = camera.WorldToScreen(world);
            
            // Origin should be at center of canvas
            AssertEqual(screen.X, 400.0f, 1.0f, "X should be at center");
            AssertEqual(screen.Y, 300.0f, 1.0f, "Y should be at center");
        });

        runner.AddTest(L"Camera_ScreenToWorld_Center", []() {
            Camera camera;
            camera.SetCanvasSize(800, 600);
            
            ScreenPoint screen{ 400, 300 };
            WorldPoint world = camera.ScreenToWorld(screen);
            
            AssertEqual(world.X, 0.0, 1.0, "Center screen X should map to world 0");
            AssertEqual(world.Y, 0.0, 1.0, "Center screen Y should map to world 0");
        });

        runner.AddTest(L"Camera_ZoomAffectsScale", []() {
            Camera camera;
            camera.SetCanvasSize(800, 600);
            
            WorldPoint testPoint{ 1000, 1000 };
            
            ScreenPoint before = camera.WorldToScreen(testPoint);
            camera.SetZoom(2.0);
            ScreenPoint after = camera.WorldToScreen(testPoint);
            
            // After 2x zoom, point should be further from center
            double distBefore = std::sqrt(std::pow(before.X - 400, 2) + std::pow(before.Y - 300, 2));
            double distAfter = std::sqrt(std::pow(after.X - 400, 2) + std::pow(after.Y - 300, 2));
            
            AssertTrue(distAfter > distBefore * 1.5, "Zoomed point should be further from center");
        });

        runner.AddTest(L"Camera_PanMovesView", []() {
            Camera camera;
            camera.SetCanvasSize(800, 600);
            
            WorldPoint origin{ 0, 0 };
            ScreenPoint before = camera.WorldToScreen(origin);
            
            camera.Pan(100, 50);
            ScreenPoint after = camera.WorldToScreen(origin);
            
            AssertEqual(after.X - before.X, 100.0f, 1.0f, "Pan X should shift by 100");
            AssertEqual(after.Y - before.Y, 50.0f, 1.0f, "Pan Y should shift by 50");
        });

        runner.AddTest(L"Camera_RoundTrip", []() {
            Camera camera;
            camera.SetCanvasSize(800, 600);
            camera.SetZoom(1.5);
            camera.Pan(100, -50);
            
            WorldPoint original{ 500, -300 };
            ScreenPoint screen = camera.WorldToScreen(original);
            WorldPoint roundTrip = camera.ScreenToWorld(screen);
            
            AssertEqual(original.X, roundTrip.X, 0.1, "X should round-trip");
            AssertEqual(original.Y, roundTrip.Y, 0.1, "Y should round-trip");
        });

        return runner.Run(L"Camera Tests");
    }

    // ============================================================================
    // Wall Tests
    // ============================================================================

    inline TestSuite RunWallTests()
    {
        TestRunner runner;

        runner.AddTest(L"Wall_Length", []() {
            Wall wall({ 0, 0 }, { 1000, 0 }, 200);
            AssertEqual(wall.GetLength(), 1000.0, 0.1, "Wall length should be 1000");
        });

        runner.AddTest(L"Wall_LengthDiagonal", []() {
            Wall wall({ 0, 0 }, { 1000, 1000 }, 200);
            double expected = std::sqrt(2000000.0); // sqrt(1000^2 + 1000^2)
            AssertEqual(wall.GetLength(), expected, 0.1, "Diagonal length incorrect");
        });

        runner.AddTest(L"Wall_Area", []() {
            Wall wall({ 0, 0 }, { 2000, 0 }, 200);
            wall.SetHeight(2800);
            double expected = 2000.0 * 2800.0;
            AssertEqual(wall.GetArea(), expected, 1.0, "Wall area incorrect");
        });

        runner.AddTest(L"Wall_Volume", []() {
            Wall wall({ 0, 0 }, { 2000, 0 }, 200);
            wall.SetHeight(2800);
            double expected = 2000.0 * 200.0 * 2800.0;
            AssertEqual(wall.GetVolume(), expected, 1.0, "Wall volume incorrect");
        });

        runner.AddTest(L"Wall_CornerPoints", []() {
            Wall wall({ 0, 0 }, { 1000, 0 }, 200);
            
            WorldPoint p1, p2, p3, p4;
            wall.GetCornerPoints(p1, p2, p3, p4);
            
            // For horizontal wall, corners should be offset in Y
            AssertEqual(p1.Y, 100.0, 1.0, "p1.Y should be +100");
            AssertEqual(p2.Y, -100.0, 1.0, "p2.Y should be -100");
        });

        runner.AddTest(L"Wall_HitTest", []() {
            Wall wall({ 0, 0 }, { 1000, 0 }, 200);
            
            // Point on wall
            AssertTrue(wall.HitTest({ 500, 0 }, 10), "Center should hit");
            
            // Point off wall
            AssertFalse(wall.HitTest({ 500, 200 }, 10), "Point 200 away should miss");
        });

        runner.AddTest(L"Wall_WorkState", []() {
            Wall wall({ 0, 0 }, { 1000, 0 }, 200);
            
            wall.SetWorkState(WorkStateNative::Demolish);
            AssertTrue(wall.GetWorkState() == WorkStateNative::Demolish, "WorkState should be Demolish");
            
            wall.SetWorkState(WorkStateNative::New);
            AssertTrue(wall.GetWorkState() == WorkStateNative::New, "WorkState should be New");
        });

        runner.AddTest(L"Wall_ThicknessClamp", []() {
            Wall wall({ 0, 0 }, { 1000, 0 }, 200);
            
            wall.SetThickness(10); // Below minimum
            AssertTrue(wall.GetThickness() >= 50, "Thickness should be clamped to min 50");
            
            wall.SetThickness(2000); // Above maximum
            AssertTrue(wall.GetThickness() <= 1000, "Thickness should be clamped to max 1000");
        });

        return runner.Run(L"Wall Tests");
    }

    // ============================================================================
    // WallType Tests
    // ============================================================================

    inline TestSuite RunWallTypeTests()
    {
        TestRunner runner;

        runner.AddTest(L"WallType_Empty", []() {
            WallType type(L"Empty");
            AssertEqual(static_cast<int>(type.GetLayerCount()), 0, "Empty type should have 0 layers");
            AssertEqual(type.GetTotalThickness(), 0.0, 0.1, "Empty type thickness should be 0");
        });

        runner.AddTest(L"WallType_SingleLayer", []() {
            WallType type(L"Single");
            type.AddLayer({ L"Core", 200.0 });
            
            AssertEqual(static_cast<int>(type.GetLayerCount()), 1, "Should have 1 layer");
            AssertEqual(type.GetTotalThickness(), 200.0, 0.1, "Thickness should be 200");
        });

        runner.AddTest(L"WallType_MultipleLayers", []() {
            WallType type(L"Multi");
            type.AddLayer({ L"Finish Ext", 15.0 });
            type.AddLayer({ L"Core", 200.0 });
            type.AddLayer({ L"Finish Int", 15.0 });
            
            AssertEqual(static_cast<int>(type.GetLayerCount()), 3, "Should have 3 layers");
            AssertEqual(type.GetTotalThickness(), 230.0, 0.1, "Total thickness should be 230");
        });

        runner.AddTest(L"WallType_CoreThickness", []() {
            WallType type(L"WithFinish");
            type.AddLayer({ L"������� ��������", 15.0 });
            type.AddLayer({ L"������", 250.0 });
            type.AddLayer({ L"������� ����������", 15.0 });
            
            // Core thickness should exclude "�����*" layers
            AssertEqual(type.GetCoreThickness(), 250.0, 0.1, "Core thickness should be 250");
        });

        runner.AddTest(L"WallType_RemoveLayer", []() {
            WallType type(L"Test");
            type.AddLayer({ L"Layer1", 100.0 });
            type.AddLayer({ L"Layer2", 100.0 });
            
            type.RemoveLayer(0);
            AssertEqual(static_cast<int>(type.GetLayerCount()), 1, "Should have 1 layer after removal");
        });

        return runner.Run(L"WallType Tests");
    }

    // ============================================================================
    // Dimension Tests
    // ============================================================================

    inline TestSuite RunDimensionTests()
    {
        TestRunner runner;

        runner.AddTest(L"Dimension_Value", []() {
            Dimension dim({ 0, 0 }, { 1000, 0 });
            AssertEqual(dim.GetValueMm(), 1000.0, 0.1, "Dimension value should be 1000");
        });

        runner.AddTest(L"Dimension_DiagonalValue", []() {
            Dimension dim({ 0, 0 }, { 1000, 1000 });
            double expected = std::sqrt(2000000.0);
            AssertEqual(dim.GetValueMm(), expected, 0.1, "Diagonal dimension incorrect");
        });

        runner.AddTest(L"Dimension_Offset", []() {
            Dimension dim({ 0, 0 }, { 1000, 0 });
            dim.SetOffset(500);
            AssertEqual(dim.GetOffset(), 500.0, 0.1, "Offset should be 500");
        });

        runner.AddTest(L"Dimension_HitTest", []() {
            Dimension dim({ 0, 0 }, { 1000, 0 });
            dim.SetOffset(100);
            
            // Point on dimension line (offset by 100 in Y)
            AssertTrue(dim.HitTest({ 500, 100 }, 20), "Should hit dimension line");
        });

        runner.AddTest(L"Dimension_ManualType", []() {
            Dimension dim({ 0, 0 }, { 1000, 0 });
            AssertTrue(dim.IsManual(), "Default dimension should be manual");
        });

        return runner.Run(L"Dimension Tests");
    }

    // ============================================================================
    // Estimation Tests
    // ============================================================================

    inline TestSuite RunEstimationTests()
    {
        TestRunner runner;

        runner.AddTest(L"Estimation_EmptyDocument", []() {
            DocumentModel doc;
            EstimationEngine engine;
            EstimationSettings settings;
            
            auto result = engine.Calculate(doc, settings);
            
            AssertEqual(result.GrandTotal, 0.0, 0.1, "Empty document should have 0 total");
            AssertEqual(static_cast<int>(result.Items.size()), 0, "Should have no items");
        });

        runner.AddTest(L"Estimation_DemolitionWall", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 2000, 0 }, 200);
            wall->SetWorkState(WorkStateNative::Demolish);
            wall->SetHeight(2800);
            
            EstimationEngine engine;
            EstimationSettings settings;
            settings.IncludeDemolition = true;
            settings.IncludeNew = false;
            
            auto result = engine.Calculate(doc, settings);
            
            AssertTrue(result.DemolitionSubtotal > 0, "Demolition total should be > 0");
            AssertTrue(result.ConstructionSubtotal == 0, "Construction total should be 0");
        });

        runner.AddTest(L"Estimation_NewWall", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 2000, 0 }, 200);
            wall->SetWorkState(WorkStateNative::New);
            wall->SetHeight(2800);
            
            EstimationEngine engine;
            EstimationSettings settings;
            settings.IncludeDemolition = false;
            settings.IncludeNew = true;
            
            auto result = engine.Calculate(doc, settings);
            
            AssertTrue(result.ConstructionSubtotal > 0, "Construction total should be > 0");
            AssertTrue(result.DemolitionSubtotal == 0, "Demolition total should be 0");
        });

        runner.AddTest(L"Estimation_Contingency", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 2000, 0 }, 200);
            wall->SetWorkState(WorkStateNative::New);
            wall->SetHeight(2800);
            
            EstimationEngine engine;
            EstimationSettings settings;
            settings.ContingencyPercent = 10.0;
            
            auto result = engine.Calculate(doc, settings);
            
            double expectedContingency = result.Subtotal * 0.10;
            AssertEqual(result.Contingency, expectedContingency, 1.0, "Contingency should be 10% of subtotal");
        });

        runner.AddTest(L"Estimation_GrandTotal", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 2000, 0 }, 200);
            wall->SetWorkState(WorkStateNative::New);
            wall->SetHeight(2800);
            
            EstimationEngine engine;
            EstimationSettings settings;
            
            auto result = engine.Calculate(doc, settings);
            
            double expectedTotal = result.Subtotal + result.Contingency;
            AssertEqual(result.GrandTotal, expectedTotal, 1.0, "GrandTotal should be Subtotal + Contingency");
        });

        return runner.Run(L"Estimation Tests");
    }

    // ============================================================================
    // DocumentModel Tests
    // ============================================================================

    inline TestSuite RunDocumentTests()
    {
        TestRunner runner;

        runner.AddTest(L"Document_AddWall", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 1000, 0 }, 200);
            
            AssertTrue(wall != nullptr, "AddWall should return valid pointer");
            AssertEqual(static_cast<int>(doc.GetWalls().size()), 1, "Should have 1 wall");
        });

        runner.AddTest(L"Document_RemoveWall", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 1000, 0 }, 200);
            uint64_t id = wall->GetId();
            
            doc.RemoveWall(id);
            AssertEqual(static_cast<int>(doc.GetWalls().size()), 0, "Should have 0 walls after removal");
        });

        runner.AddTest(L"Document_FindWall", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 1000, 0 }, 200);
            uint64_t id = wall->GetId();
            
            // Find wall in collection
            Wall* found = nullptr;
            for (const auto& w : doc.GetWalls())
            {
                if (w && w->GetId() == id)
                {
                    found = w.get();
                    break;
                }
            }
            AssertTrue(found == wall, "Should find the same wall");
        });

        runner.AddTest(L"Document_Selection", []() {
            DocumentModel doc;
            Wall* wall = doc.AddWall({ 0, 0 }, { 1000, 0 }, 200);
            
            doc.SetSelectedElement(wall);
            AssertTrue(wall->IsSelected(), "Wall should be selected");
            AssertTrue(doc.GetSelectedElement() == wall, "Selected element should be wall");
            
            doc.ClearSelection();
            AssertFalse(wall->IsSelected(), "Wall should not be selected after clear");
        });

        runner.AddTest(L"Document_WallTypes", []() {
            DocumentModel doc;
            
            // Default types should exist
            const auto& types = doc.GetWallTypes();
            AssertTrue(types.size() > 0, "Should have default wall types");
        });

        return runner.Run(L"Document Tests");
    }

    // ============================================================================
    // Run All Tests
    // ============================================================================

    struct AllTestsResult
    {
        std::vector<TestSuite> Suites;
        int TotalPassed{ 0 };
        int TotalFailed{ 0 };
        double TotalDurationMs{ 0.0 };

        std::wstring GetSummary() const
        {
            std::wstringstream ss;
            ss << L"=== TEST RESULTS ===\n\n";

            for (const auto& suite : Suites)
            {
                ss << L"[" << suite.Name << L"]\n";
                for (const auto& result : suite.Results)
                {
                    ss << L"  " << (result.Passed ? L"?" : L"?") << L" "
                       << result.TestName;
                    if (!result.Passed)
                    {
                        ss << L" - " << result.Message;
                    }
                    ss << L" (" << result.DurationMs << L" ms)\n";
                }
                ss << L"  Passed: " << suite.PassedCount << L"/" 
                   << (suite.PassedCount + suite.FailedCount) << L"\n\n";
            }

            ss << L"===================\n";
            ss << L"TOTAL: " << TotalPassed << L" passed, " << TotalFailed << L" failed\n";
            ss << L"Duration: " << TotalDurationMs << L" ms\n";

            return ss.str();
        }
    };

    inline AllTestsResult RunAllTests()
    {
        AllTestsResult result;

        // Run all test suites
        result.Suites.push_back(RunCameraTests());
        result.Suites.push_back(RunWallTests());
        result.Suites.push_back(RunWallTypeTests());
        result.Suites.push_back(RunDimensionTests());
        result.Suites.push_back(RunEstimationTests());
        result.Suites.push_back(RunDocumentTests());

        // Aggregate results
        for (const auto& suite : result.Suites)
        {
            result.TotalPassed += suite.PassedCount;
            result.TotalFailed += suite.FailedCount;
            result.TotalDurationMs += suite.TotalDurationMs;
        }

        return result;
    }

} // namespace winrt::estimate1::tests
