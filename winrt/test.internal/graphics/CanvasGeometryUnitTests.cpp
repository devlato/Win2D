// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may
// not use these files except in compliance with the License. You may obtain
// a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pch.h"
#include <CanvasPathBuilder.h>
#include "MockD2DRectangleGeometry.h"
#include "MockD2DEllipseGeometry.h"
#include "MockD2DRoundedRectangleGeometry.h"
#include "MockD2DPathGeometry.h"
#include "MockD2DGeometrySink.h"
#include "MockD2DTransformedGeometry.h"
#include "MockD2DGeometryGroup.h"

static const D2D1_MATRIX_3X2_F sc_someD2DTransform = { 1, 2, 3, 4, 5, 6 };
static const D2D1_MATRIX_3X2_F sc_identityD2DTransform = { 1, 0, 0, 1, 0, 0 };
static const Matrix3x2 sc_someTransform = { 1, 2, 3, 4, 5, 6 };

static const D2D1_TRIANGLE sc_triangle1 = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
static const D2D1_TRIANGLE sc_triangle2 = { { 7, 8 }, { 9, 10 }, { 11, 12 } };
static const D2D1_TRIANGLE sc_triangle3 = { { 13, 14 }, { 15, 16 }, { 17, 18 } };

TEST_CLASS(CanvasGeometryTests)
{
public: 

    struct Fixture
    {
        ComPtr<StubCanvasDevice> Device;
        std::shared_ptr<CanvasGeometryManager> Manager;

        Fixture()
            : Device(Make<StubCanvasDevice>())
            , Manager(std::make_shared<CanvasGeometryManager>())
        {
            Device->CreateRectangleGeometryMethod.AllowAnyCall(
                [](D2D1_RECT_F const&)
                {
                    return Make<MockD2DRectangleGeometry>();
                });

        }
    };

    TEST_METHOD_EX(CanvasGeometry_ImplementsExpectedInterfaces)
    {
        Fixture f;

        auto canvasGeometry = f.Manager->Create(f.Device.Get(), Rect{});

        ASSERT_IMPLEMENTS_INTERFACE(canvasGeometry, ICanvasGeometry);
        ASSERT_IMPLEMENTS_INTERFACE(canvasGeometry, ABI::Windows::Foundation::IClosable);
    }

    TEST_METHOD_EX(CanvasGeometry_CreatedWithCorrectD2DRectangleResource)
    {
        Fixture f;

        Rect expectedRect{ 1, 2, 3, 4 };
        const D2D1_RECT_F expectedD2DRect = 
            D2D1::RectF(expectedRect.X, expectedRect.Y, expectedRect.X + expectedRect.Width, expectedRect.Y + expectedRect.Height);
        
        f.Device->CreateRectangleGeometryMethod.SetExpectedCalls(1, 
            [expectedD2DRect](D2D1_RECT_F const& rect)
            {
                Assert::AreEqual(expectedD2DRect, rect);
                return Make<MockD2DRectangleGeometry>();
            });

        f.Manager->Create(f.Device.Get(), expectedRect);
    }

    TEST_METHOD_EX(CanvasGeometry_CreatedWithCorrectD2DEllipseResource)
    {
        Fixture f;

        D2D1_ELLIPSE testEllipse = D2D1::Ellipse(D2D1::Point2F(1, 2), 3, 4);

        f.Device->CreateEllipseGeometryMethod.SetExpectedCalls(1,
            [testEllipse](D2D1_ELLIPSE const& ellipse)
            {
                Assert::AreEqual(testEllipse, ellipse);
                return Make<MockD2DEllipseGeometry>();
            });
            
        f.Manager->Create(f.Device.Get(), Vector2{ testEllipse.point.x, testEllipse.point.y }, testEllipse.radiusX, testEllipse.radiusY);
    }

    TEST_METHOD_EX(CanvasGeometry_CreatedWithCorrectD2DRoundedRectangleResource)
    {
        Fixture f;

        Rect expectedRect{ 1, 2, 3, 4 };
        const D2D1_RECT_F expectedD2DRect =
            D2D1::RectF(expectedRect.X, expectedRect.Y, expectedRect.X + expectedRect.Width, expectedRect.Y + expectedRect.Height);
        float expectedRadiusX = 5;
        float expectedRadiusY = 6;

        f.Device->CreateRoundedRectangleGeometryMethod.SetExpectedCalls(1,
            [expectedD2DRect, expectedRadiusX, expectedRadiusY](D2D1_ROUNDED_RECT const& roundedRect)
            {
                Assert::AreEqual(D2D1::RoundedRect(expectedD2DRect, expectedRadiusX, expectedRadiusY), roundedRect);
                return Make<MockD2DRoundedRectangleGeometry>();
            });

        f.Manager->Create(f.Device.Get(), expectedRect, expectedRadiusX, expectedRadiusY);
    }

    class GeometryGroupFixture : public Fixture
    {
        struct Resource
        {
            ComPtr<MockD2DRectangleGeometry> D2DGeometry;
            ComPtr<ICanvasGeometry> CanvasGeometry;
        };
        std::vector<Resource> m_resources;
        std::vector<ICanvasGeometry*> m_rawCanvasData;

    public:

        GeometryGroupFixture(UINT resourceCount)
        {
            for (UINT i = 0; i < resourceCount; ++i)
            {
                Resource r;
                r.D2DGeometry = Make<MockD2DRectangleGeometry>();
                r.CanvasGeometry = Manager->GetOrCreate(Device.Get(), r.D2DGeometry.Get());
                m_resources.push_back(r);

                m_rawCanvasData.push_back(r.CanvasGeometry.Get());
            }
        }

        ICanvasGeometry** GetGeometries()
        {
            return &m_rawCanvasData[0];
        }

        void ValidateGeometries(ID2D1Geometry** geometries, uint32_t geometryCount) const
        {
            assert(m_rawCanvasData.size() < UINT_MAX);
            Assert::AreEqual(static_cast<uint32_t>(m_rawCanvasData.size()), geometryCount);

            for (UINT i = 0; i < geometryCount; ++i)
            {
                Assert::AreEqual(static_cast<ID2D1Geometry*>(m_resources[i].D2DGeometry.Get()), geometries[i]);
            }
        }

        void DeleteResourceAtIndex(UINT i)
        {
            assert(i < m_rawCanvasData.size());
            m_resources[i] = Resource{};
            m_rawCanvasData[i] = nullptr;
        }
    };

    TEST_METHOD_EX(CanvasGeometry_CreatedWithCorrectD2DGeometryGroup)
    {
        int geometryCounts[] = { 1, 3 };

        for (int geometryCount : geometryCounts)
        {
            GeometryGroupFixture f(geometryCount);

            f.Device->CreateGeometryGroupMethod.SetExpectedCalls(1,
                [&f](D2D1_FILL_MODE fillMode, ID2D1Geometry** geometries, uint32_t geometryCount)
                {
                    Assert::AreEqual(D2D1_FILL_MODE_WINDING, fillMode);

                    f.ValidateGeometries(geometries, geometryCount);

                    return Make<MockD2DGeometryGroup>();
                });

            f.Manager->Create(f.Device.Get(), geometryCount, f.GetGeometries(), CanvasFilledRegionDetermination::Winding);
        }
    }

    TEST_METHOD_EX(CanvasGeometry_ZeroSizedGeometryGroup_NullInputArray)
    {
        Fixture f;

        auto d2dGeometry = Make<MockD2DRectangleGeometry>();
        ComPtr<ICanvasGeometry> canvasGeometry = f.Manager->GetOrCreate(f.Device.Get(), d2dGeometry.Get()); 

        f.Device->CreateGeometryGroupMethod.SetExpectedCalls(1,
            [&f](D2D1_FILL_MODE fillMode, ID2D1Geometry** geometries, uint32_t geometryCount)
            {
                Assert::AreEqual(0u, geometryCount);

                Assert::IsNotNull(geometries);

                return Make<MockD2DGeometryGroup>();
            });

        auto geometryGroup = f.Manager->Create(f.Device.Get(), 0, nullptr, CanvasFilledRegionDetermination::Winding);
        Assert::IsNotNull(geometryGroup.Get());
    }

    TEST_METHOD_EX(CanvasGeometry_ZeroSizedGeometryGroup_NonNullInputArray)
    {
        Fixture f;

        auto d2dGeometry = Make<MockD2DRectangleGeometry>();
        ComPtr<ICanvasGeometry> canvasGeometry = f.Manager->GetOrCreate(f.Device.Get(), d2dGeometry.Get());

        f.Device->CreateGeometryGroupMethod.SetExpectedCalls(1,
            [&f](D2D1_FILL_MODE fillMode, ID2D1Geometry** geometries, uint32_t geometryCount)
            {
                Assert::AreEqual(0u, geometryCount);

                Assert::IsNotNull(geometries);

                return Make<MockD2DGeometryGroup>();
            });

        auto geometryGroup = f.Manager->Create(f.Device.Get(), 0, canvasGeometry.GetAddressOf(), CanvasFilledRegionDetermination::Winding);
        Assert::IsNotNull(geometryGroup.Get());
    }

    TEST_METHOD_EX(CanvasGeometry_GeometryGroupWithOneNullGeometry)
    {
        GeometryGroupFixture f(3);

        f.DeleteResourceAtIndex(2);

        ExpectHResultException(E_INVALIDARG, [&]{ f.Manager->Create(f.Device.Get(), 3, f.GetGeometries(), CanvasFilledRegionDetermination::Winding); });
    }

    class GeometryOperationsFixture_DoesNotOutputToTempPathBuilder : public Fixture
    {
        ComPtr<StubD2DFactoryWithCreateStrokeStyle> m_factory;

    public:
        ComPtr<MockD2DRectangleGeometry> D2DRectangleGeometry;
        ComPtr<ICanvasGeometry> RectangleGeometry;

        ComPtr<MockD2DEllipseGeometry> D2DEllipseGeometry;
        ComPtr<ICanvasGeometry> EllipseGeometry;

        ComPtr<ICanvasStrokeStyle> StrokeStyle;

        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder()
            : D2DRectangleGeometry(Make<MockD2DRectangleGeometry>())
            , D2DEllipseGeometry(Make<MockD2DEllipseGeometry>())
            , StrokeStyle(Make<CanvasStrokeStyle>())
            , m_factory(Make<StubD2DFactoryWithCreateStrokeStyle>())
        {
            RectangleGeometry = Manager->GetOrCreate(Device.Get(), D2DRectangleGeometry.Get());

            EllipseGeometry = Manager->GetOrCreate(Device.Get(), D2DEllipseGeometry.Get());

            StrokeStyle->put_LineJoin(CanvasLineJoin::MiterOrBevel);            

            D2DRectangleGeometry->GetFactoryMethod.AllowAnyCall(
                [this](ID2D1Factory** out)
                {
                    m_factory.CopyTo(out);
                });
            
            Device->CreatePathGeometryMethod.AllowAnyCall(
                []()
                {
                    auto pathGeometry = Make<MockD2DPathGeometry>();

                    pathGeometry->OpenMethod.AllowAnyCall(
                        [](ID2D1GeometrySink** out)
                        {
                            auto geometrySink = Make<MockD2DGeometrySink>();
                            return geometrySink.CopyTo(out);
                        });

                    return pathGeometry;
                });
        }

        void VerifyStrokeStyle()
        {
            Assert::AreEqual(m_factory->m_lineJoin, D2D1_LINE_JOIN_MITER_OR_BEVEL);
        }
    };

    class GeometryOperationsFixture_OutputsToTempPathBuilder : public GeometryOperationsFixture_DoesNotOutputToTempPathBuilder
    {
    public:
        ComPtr<MockD2DPathGeometry> TemporaryPathGeometry;
        ComPtr<MockD2DGeometrySink> SinkForTemporaryPath;

        GeometryOperationsFixture_OutputsToTempPathBuilder()
            : TemporaryPathGeometry(Make<MockD2DPathGeometry>())
            , SinkForTemporaryPath(Make<MockD2DGeometrySink>())
        {
            SinkForTemporaryPath->CloseMethod.AllowAnyCall();

            Device->CreatePathGeometryMethod.SetExpectedCalls(1,
                [this]()
                {
                    TemporaryPathGeometry->OpenMethod.SetExpectedCalls(1,
                        [this](ID2D1GeometrySink** out)
                        {
                            return SinkForTemporaryPath.CopyTo(out);
                        });

                    return TemporaryPathGeometry;
                });
        }

        void VerifyResult(ComPtr<ICanvasGeometry> const& resultCanvasGeometry, bool expectStrokeStyle = false)
        {
            auto d2dResource = GetWrappedResource<ID2D1Geometry>(resultCanvasGeometry);

            Assert::AreEqual<ID2D1Geometry*>(d2dResource.Get(), TemporaryPathGeometry.Get());

            if (expectStrokeStyle)
                VerifyStrokeStyle();
        }
    };

    TEST_METHOD_EX(CanvasGeometry_Combine)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->CombineWithGeometryMethod.SetExpectedCalls(1,
            [&](ID2D1Geometry* geometry, D2D1_COMBINE_MODE combineMode, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(static_cast<ID2D1Geometry*>(f.D2DEllipseGeometry.Get()), geometry);
                Assert::AreEqual(D2D1_COMBINE_MODE_INTERSECT, combineMode);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->CombineWith(f.EllipseGeometry.Get(), sc_someTransform, CanvasGeometryCombine::Intersect, &g));
        
        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_CombineWithUsingFlatteningTolerance)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->CombineWithGeometryMethod.SetExpectedCalls(1,
            [&](ID2D1Geometry* geometry, D2D1_COMBINE_MODE combineMode, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
        {
            Assert::AreEqual(static_cast<ID2D1Geometry*>(f.D2DEllipseGeometry.Get()), geometry);
            Assert::AreEqual(D2D1_COMBINE_MODE_INTERSECT, combineMode);
            Assert::AreEqual(sc_someD2DTransform, *transform);
            Assert::AreEqual(2.0f, tol);
            Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
            return S_OK;
        });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->CombineWithUsingFlatteningTolerance(f.EllipseGeometry.Get(), sc_someTransform, CanvasGeometryCombine::Intersect, 2.0f, &g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_Combine_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CombineWith(nullptr, Matrix3x2{}, CanvasGeometryCombine::Union, &g));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CombineWith(f.RectangleGeometry.Get(), Matrix3x2{}, CanvasGeometryCombine::Union, nullptr));

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CombineWithUsingFlatteningTolerance(nullptr, Matrix3x2{}, CanvasGeometryCombine::Union, 0, &g));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CombineWithUsingFlatteningTolerance(f.RectangleGeometry.Get(), Matrix3x2{}, CanvasGeometryCombine::Union, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_Stroke)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->WidenMethod.SetExpectedCalls(1,
            [&](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                Assert::IsNull(strokeStyle);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->Stroke(5.0f, &g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_StrokeWithStrokeStyle)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->WidenMethod.SetExpectedCalls(1,
            [&](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                Assert::IsNotNull(strokeStyle);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->StrokeWithStrokeStyle(5.0f, f.StrokeStyle.Get(), &g));

        f.VerifyResult(g, true);
    }

    TEST_METHOD_EX(CanvasGeometry_StrokeWithAllOptions)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->WidenMethod.SetExpectedCalls(1,
            [&](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                Assert::IsNotNull(strokeStyle);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->StrokeWithAllOptions(5.0f, f.StrokeStyle.Get(), sc_someTransform, 2.0f, &g));

        f.VerifyResult(g, true);
    }

    TEST_METHOD_EX(CanvasGeometry_Stroke_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Stroke(0, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeWithStrokeStyle(0, nullptr, &g));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeWithStrokeStyle(0, f.StrokeStyle.Get(), nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeWithAllOptions(0, nullptr, Matrix3x2{}, 0, &g));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeWithAllOptions(0, f.StrokeStyle.Get(), Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_Outline)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->OutlineMethod.SetExpectedCalls(1,
            [&](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->Outline(&g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_OutlineWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->OutlineMethod.SetExpectedCalls(1,
            [&](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->OutlineWithTransformAndFlatteningTolerance(sc_someTransform, 2.0f, &g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_Outline_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Outline(nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->OutlineWithTransformAndFlatteningTolerance(Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_Simplify)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->SimplifyMethod.SetExpectedCalls(1,
            [&](D2D1_GEOMETRY_SIMPLIFICATION_OPTION simplification, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES, simplification);
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->Simplify(CanvasGeometrySimplification::Lines, &g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_SimplifyWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_OutputsToTempPathBuilder f;

        f.D2DRectangleGeometry->SimplifyMethod.SetExpectedCalls(1,
            [&](D2D1_GEOMETRY_SIMPLIFICATION_OPTION simplification, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, ID2D1SimplifiedGeometrySink* sink)
            {
                Assert::AreEqual(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_LINES, simplification);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                Assert::AreEqual<ID2D1SimplifiedGeometrySink*>(f.SinkForTemporaryPath.Get(), sink);
                return S_OK;
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->SimplifyWithTransformAndFlatteningTolerance(CanvasGeometrySimplification::Lines, sc_someTransform, 2.0f, &g));

        f.VerifyResult(g);
    }

    TEST_METHOD_EX(CanvasGeometry_Simplify_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Simplify(CanvasGeometrySimplification::Lines, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->SimplifyWithTransformAndFlatteningTolerance(CanvasGeometrySimplification::Lines, Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_Transform)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.Device->CreateTransformedGeometryMethod.SetExpectedCalls(1,
            [&](ID2D1Geometry* geometry, D2D1_MATRIX_3X2_F* transform)
            {
                Assert::AreEqual(static_cast<ID2D1Geometry*>(f.D2DRectangleGeometry.Get()), geometry);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                return Make<MockD2DTransformedGeometry>();
            });

        ComPtr<ICanvasGeometry> g;
        Assert::AreEqual(S_OK, f.RectangleGeometry->Transform(sc_someTransform, &g));
    }

    TEST_METHOD_EX(CanvasGeometry_Transform_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Transform(Matrix3x2{}, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_CompareWith)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        struct GeometryRelationEnumMapping
        {
            CanvasGeometryRelation CanvasEnum;
            D2D1_GEOMETRY_RELATION D2DEnum;
        } testCases[] = 
        {
            { CanvasGeometryRelation::Disjoint, D2D1_GEOMETRY_RELATION_DISJOINT },
            { CanvasGeometryRelation::Contained, D2D1_GEOMETRY_RELATION_IS_CONTAINED },
            { CanvasGeometryRelation::Contains, D2D1_GEOMETRY_RELATION_CONTAINS },
            { CanvasGeometryRelation::Overlap, D2D1_GEOMETRY_RELATION_OVERLAP },
        };

        for (auto testCase : testCases)
        {
            f.D2DRectangleGeometry->CompareWithGeometryMethod.SetExpectedCalls(1,
                [&](ID2D1Geometry* otherGeometry, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, D2D1_GEOMETRY_RELATION* relation)
                {
                    Assert::AreEqual(static_cast<ID2D1Geometry*>(f.D2DEllipseGeometry.Get()), otherGeometry);
                    Assert::AreEqual(sc_identityD2DTransform, *transform);
                    Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);

                    *relation = testCase.D2DEnum;

                    return S_OK;
                });

            CanvasGeometryRelation result;
            Assert::AreEqual(S_OK, f.RectangleGeometry->CompareWith(f.EllipseGeometry.Get(), &result));
            Assert::AreEqual(testCase.CanvasEnum, result);
        }

    }

    TEST_METHOD_EX(CanvasGeometry_CompareWithUsingTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->CompareWithGeometryMethod.SetExpectedCalls(1,
            [&](ID2D1Geometry* otherGeometry, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, D2D1_GEOMETRY_RELATION* relation)
            {
                Assert::AreEqual(static_cast<ID2D1Geometry*>(f.D2DEllipseGeometry.Get()), otherGeometry);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);

                *relation = D2D1_GEOMETRY_RELATION_DISJOINT;

                return S_OK;
            });

        CanvasGeometryRelation result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->CompareWithUsingTransformAndFlatteningTolerance(f.EllipseGeometry.Get(), sc_someTransform, 2.0f, &result));
        Assert::AreEqual(CanvasGeometryRelation::Disjoint, result);
    }

    TEST_METHOD_EX(CanvasGeometry_CompareWith_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComPtr<ICanvasGeometry> g;

        CanvasGeometryRelation r;
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CompareWith(nullptr, &r));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CompareWith(f.RectangleGeometry.Get(), nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CompareWithUsingTransformAndFlatteningTolerance(nullptr, Matrix3x2{}, 0, &r));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->CompareWithUsingTransformAndFlatteningTolerance(f.RectangleGeometry.Get(), Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeArea)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputeAreaMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, float* area)
            {
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *area = 123.0f;
                return S_OK;
            });

        float result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeArea(&result));
        Assert::AreEqual(123.0f, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeAreaWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputeAreaMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, float* area)
            {
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                *area = 123.0f;
                return S_OK;
            });

        float result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeAreaWithTransformAndFlatteningTolerance(sc_someTransform, 2.0f, &result));
        Assert::AreEqual(123.0f, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeArea_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeArea(nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeAreaWithTransformAndFlatteningTolerance(Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePathLength)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputeLengthMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, float* area)
            {
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *area = 123.0f;
                return S_OK;
            });

        float result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputePathLength(&result));
        Assert::AreEqual(123.0f, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePathLengthWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputeLengthMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, float* length)
            {
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                *length = 123.0f;
                return S_OK;
            });

        float result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputePathLengthWithTransformAndFlatteningTolerance(sc_someTransform, 2.0f, &result));
        Assert::AreEqual(123.0f, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePathLength_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePathLength(nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePathLengthWithTransformAndFlatteningTolerance(Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePointOnPath)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputePointAtLengthMethod.SetExpectedCalls(1,
            [=](FLOAT length, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, D2D1_POINT_2F* position, D2D1_POINT_2F* tangent)
            {
                Assert::AreEqual(99.0f, length);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *position = D2D1_POINT_2F{ 12, 34 };
                Assert::IsNotNull(tangent);
                return S_OK;
            });

        Vector2 point;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputePointOnPath(99.0f, &point));
        Assert::AreEqual(Vector2{ 12, 34 }, point);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePointOnPathWithTangent)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputePointAtLengthMethod.SetExpectedCalls(1,
            [&](FLOAT length, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, D2D1_POINT_2F* position, D2D1_POINT_2F* tangent)
            {
                Assert::AreEqual(99.0f, length);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *position = D2D1_POINT_2F{ 12, 34 };
                *tangent = D2D1_POINT_2F{ 56, 78 };
                return S_OK;
            });

        Vector2 tangent, point;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputePointOnPathWithTangent(99.0f, &tangent, &point));
        Assert::AreEqual(Vector2{ 12, 34 }, point);
        Assert::AreEqual(Vector2{ 56, 78 }, tangent);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePointOnPathWithTransformAndFlatteningToleranceAndTangent)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->ComputePointAtLengthMethod.SetExpectedCalls(1,
            [=](FLOAT length, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, D2D1_POINT_2F* position, D2D1_POINT_2F* tangent)
            {
                Assert::AreEqual(99.0f, length);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                *position = D2D1_POINT_2F{ 12, 34 };
                *tangent = D2D1_POINT_2F{ 56, 78 };
                return S_OK;
            });

        Vector2 tangent, point;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputePointOnPathWithTransformAndFlatteningToleranceAndTangent(99.0f, sc_someTransform, 2.0f, &tangent, &point));
        Assert::AreEqual(Vector2{ 12, 34 }, point);
        Assert::AreEqual(Vector2{ 56, 78 }, tangent);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputePointOnPath_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        Vector2 pt;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePointOnPath(0, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePointOnPathWithTangent(0, &pt, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePointOnPathWithTangent(0, nullptr, &pt));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePointOnPathWithTransformAndFlatteningToleranceAndTangent(0, Matrix3x2{}, 0, &pt, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputePointOnPathWithTransformAndFlatteningToleranceAndTangent(0, Matrix3x2{}, 0, nullptr, &pt));
    }

    TEST_METHOD_EX(CanvasGeometry_FillContainsPoint)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->FillContainsPointMethod.SetExpectedCalls(1,
            [=](D2D1_POINT_2F point, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, BOOL* contains)
            {
                Assert::AreEqual(D2D1_POINT_2F{ 123, 456 }, point);
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *contains = true;
                return S_OK;
            });

        boolean result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->FillContainsPoint(Vector2{ 123, 456 }, &result));
        Assert::IsTrue(!!result);
    }

    TEST_METHOD_EX(CanvasGeometry_FillContainsPointWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->FillContainsPointMethod.SetExpectedCalls(1,
            [=](D2D1_POINT_2F point, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, BOOL* contains)
            {
                Assert::AreEqual(D2D1_POINT_2F{ 123, 456 }, point);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                *contains = true;
                return S_OK;
            });

        boolean result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->FillContainsPointWithTransformAndFlatteningTolerance(Vector2{ 123, 456 }, sc_someTransform, 2.0f, &result));
        Assert::IsTrue(!!result);
    }

    TEST_METHOD_EX(CanvasGeometry_FillContainsPoint_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->FillContainsPoint(Vector2{}, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->FillContainsPointWithTransformAndFlatteningTolerance(Vector2{}, Matrix3x2{}, 0, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeBounds)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->GetBoundsMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, D2D1_RECT_F* bounds)
            {
                Assert::AreEqual(sc_identityD2DTransform, *transform);
                *bounds = D2D1_RECT_F{ 1, 2, 1 + 3, 2 + 4 };
                return S_OK;
            });

        Rect result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeBounds(&result));
        Assert::AreEqual(Rect{ 1, 2, 3, 4 }, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeBoundsWithTransform)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->GetBoundsMethod.SetExpectedCalls(1,
            [=](CONST D2D1_MATRIX_3X2_F* transform, D2D1_RECT_F* bounds)
            {
                Assert::AreEqual(sc_someD2DTransform, *transform);
                *bounds = D2D1_RECT_F{ 1, 2, 1 + 3, 2 + 4 };
                return S_OK;
            });

        Rect result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeBoundsWithTransform(sc_someTransform, &result));
        Assert::AreEqual(Rect{ 1, 2, 3, 4 }, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeBounds_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeBounds(nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeBoundsWithTransform(Matrix3x2{}, nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeStrokeBounds)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->GetWidenedBoundsMethod.SetExpectedCalls(1,
            [=](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT flatteningTolerance, D2D1_RECT_F* bounds)
            {
                Assert::AreEqual(strokeWidth, 5.0f);
                Assert::IsNull(strokeStyle);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, flatteningTolerance);
                *bounds = D2D1_RECT_F{ 1, 2, 1 + 3, 2 + 4 };
                return S_OK;
            });

        Rect result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeStrokeBounds(5.0f, &result));
        Assert::AreEqual(Rect{ 1, 2, 3, 4 }, result);
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeStrokeBoundsWithStrokeStyle)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->GetWidenedBoundsMethod.SetExpectedCalls(1,
            [&](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT flatteningTolerance, D2D1_RECT_F* bounds)
            {
                Assert::AreEqual(strokeWidth, 5.0f);
                Assert::IsNotNull(strokeStyle);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, flatteningTolerance);
                *bounds = D2D1_RECT_F{ 1, 2, 1 + 3, 2 + 4 };
                return S_OK;
            });

        Rect result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeStrokeBoundsWithStrokeStyle(5.0f, f.StrokeStyle.Get(), &result));
        Assert::AreEqual(Rect{ 1, 2, 3, 4 }, result);
        f.VerifyStrokeStyle();
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeStrokeBoundsWithAllOptions)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->GetWidenedBoundsMethod.SetExpectedCalls(1,
            [=](FLOAT strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT flatteningTolerance, D2D1_RECT_F* bounds)
            {
                Assert::AreEqual(strokeWidth, 5.0f);
                Assert::IsNotNull(strokeStyle);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, flatteningTolerance);
                *bounds = D2D1_RECT_F{ 1, 2, 1 + 3, 2 + 4 };
                return S_OK;
            });

        Rect result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->ComputeStrokeBoundsWithAllOptions(5.0f, f.StrokeStyle.Get(), sc_someTransform, 2.0f, &result));
        Assert::AreEqual(Rect{ 1, 2, 3, 4 }, result);
        f.VerifyStrokeStyle();
    }

    TEST_METHOD_EX(CanvasGeometry_ComputeStrokeBounds_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        Rect rect;
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeStrokeBounds(0, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeStrokeBoundsWithStrokeStyle(0, nullptr, &rect));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeStrokeBoundsWithStrokeStyle(0, f.StrokeStyle.Get(), nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeStrokeBoundsWithAllOptions(0, nullptr, Matrix3x2{}, 0, &rect));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->ComputeStrokeBoundsWithAllOptions(0, f.StrokeStyle.Get(), Matrix3x2{}, 0, nullptr));
    }


    TEST_METHOD_EX(CanvasGeometry_StrokeContainsPoint)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->StrokeContainsPointMethod.SetExpectedCalls(1,
            [=](D2D1_POINT_2F point, float strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, BOOL* contains)
            {
                Assert::AreEqual(D2D1_POINT_2F{ 123, 456 }, point);
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::IsNull(strokeStyle);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *contains = true;
                return S_OK;
            });

        boolean result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->StrokeContainsPoint(Vector2{ 123, 456 }, 5.0f, &result));
        Assert::IsTrue(!!result);
    }

    TEST_METHOD_EX(CanvasGeometry_StrokeContainsPointWithStrokeStyle)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->StrokeContainsPointMethod.SetExpectedCalls(1,
            [&](D2D1_POINT_2F point, float strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, BOOL* contains)
            {
                Assert::AreEqual(D2D1_POINT_2F{ 123, 456 }, point);
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::IsNotNull(strokeStyle);
                Assert::IsNull(transform);
                Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, tol);
                *contains = true;
                return S_OK;
            });

        boolean result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->StrokeContainsPointWithStrokeStyle(Vector2{ 123, 456 }, 5.0f, f.StrokeStyle.Get(), &result));
        Assert::IsTrue(!!result);
        f.VerifyStrokeStyle();
    }

    TEST_METHOD_EX(CanvasGeometry_StrokeContainsPointWithTransformAndFlatteningTolerance)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        f.D2DRectangleGeometry->StrokeContainsPointMethod.SetExpectedCalls(1,
            [=](D2D1_POINT_2F point, float strokeWidth, ID2D1StrokeStyle* strokeStyle, CONST D2D1_MATRIX_3X2_F* transform, FLOAT tol, BOOL* contains)
            {
                Assert::AreEqual(D2D1_POINT_2F{ 123, 456 }, point);
                Assert::AreEqual(5.0f, strokeWidth);
                Assert::IsNotNull(strokeStyle);
                Assert::AreEqual(sc_someD2DTransform, *transform);
                Assert::AreEqual(2.0f, tol);
                *contains = true;
                return S_OK;
            });

        boolean result;
        Assert::AreEqual(S_OK, f.RectangleGeometry->StrokeContainsPointWithAllOptions(Vector2{ 123, 456 }, 5.0f, f.StrokeStyle.Get(), sc_someTransform, 2.0f, &result));
        Assert::IsTrue(!!result);
        f.VerifyStrokeStyle();
    }

    TEST_METHOD_EX(CanvasGeometry_StrokeContainsPoint_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        boolean b;
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeContainsPoint(Vector2{}, 0, nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeContainsPointWithStrokeStyle(Vector2{}, 0, nullptr, &b));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeContainsPointWithStrokeStyle(Vector2{}, 0, f.StrokeStyle.Get(), nullptr));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeContainsPointWithAllOptions(Vector2{}, 0, nullptr, Matrix3x2{}, 0, &b));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->StrokeContainsPointWithAllOptions(Vector2{}, 0, f.StrokeStyle.Get(), Matrix3x2{}, 0, nullptr));
    }

    struct TessellateFixture : public GeometryOperationsFixture_DoesNotOutputToTempPathBuilder
    {
        void ExpectOneTessellateCall(D2D1_MATRIX_3X2_F expectedTransform, float expectedFlatteningTolerance)
        {
            D2DRectangleGeometry->TessellateMethod.SetExpectedCalls(1,
                [=](D2D1_MATRIX_3X2_F const* transform, float flatteningTolerance, ID2D1TessellationSink* sink)
                {
                    Assert::AreEqual(expectedTransform, *transform);
                    Assert::AreEqual(expectedFlatteningTolerance, flatteningTolerance);

                    sink->AddTriangles(&sc_triangle1, 1);

                    D2D1_TRIANGLE twoTriangles[] =
                    {
                        sc_triangle2,
                        sc_triangle3,
                    };

                    sink->AddTriangles(twoTriangles, 2);

                    return S_OK;
                });
        }

        void ValidateTessellatedTriangles(ComArray<CanvasTriangleVertices>& triangles)
        {
            Assert::AreEqual(3u, triangles.GetSize());

            Assert::AreEqual(sc_triangle1, *ReinterpretAs<D2D1_TRIANGLE const*>(&triangles[0]));
            Assert::AreEqual(sc_triangle2, *ReinterpretAs<D2D1_TRIANGLE const*>(&triangles[1]));
            Assert::AreEqual(sc_triangle3, *ReinterpretAs<D2D1_TRIANGLE const*>(&triangles[2]));
        }
    };

    TEST_METHOD_EX(CanvasGeometry_Tessellate)
    {
        TessellateFixture f;
        ComArray<CanvasTriangleVertices> triangles;

        f.ExpectOneTessellateCall(sc_identityD2DTransform, D2D1_DEFAULT_FLATTENING_TOLERANCE);

        ThrowIfFailed(f.RectangleGeometry->Tessellate(triangles.GetAddressOfSize(), triangles.GetAddressOfData()));

        f.ValidateTessellatedTriangles(triangles);
    }

    TEST_METHOD_EX(CanvasGeometry_TessellateWithTransformAndFlatteningTolerance)
    {
        TessellateFixture f;
        ComArray<CanvasTriangleVertices> triangles;

        const float expectedTolerance = 23;

        f.ExpectOneTessellateCall(sc_someD2DTransform, expectedTolerance);

        ThrowIfFailed(f.RectangleGeometry->TessellateWithTransformAndFlatteningTolerance(sc_someTransform, expectedTolerance, triangles.GetAddressOfSize(), triangles.GetAddressOfData()));

        f.ValidateTessellatedTriangles(triangles);
    }

    TEST_METHOD_EX(CanvasGeometry_Tessellate_NullArgs)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;
        ComArray<CanvasTriangleVertices> t;

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Tessellate(nullptr, t.GetAddressOfData()));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->Tessellate(t.GetAddressOfSize(), nullptr));

        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->TessellateWithTransformAndFlatteningTolerance(Matrix3x2{}, 0, nullptr, t.GetAddressOfData()));
        Assert::AreEqual(E_INVALIDARG, f.RectangleGeometry->TessellateWithTransformAndFlatteningTolerance(Matrix3x2{}, 0, t.GetAddressOfSize(), nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_Closure)
    {
        GeometryOperationsFixture_DoesNotOutputToTempPathBuilder f;

        auto canvasGeometry = f.Manager->Create(f.Device.Get(), Rect{});
        auto otherCanvasGeometry = f.Manager->Create(f.Device.Get(), Rect{});
        auto pb = Make<CanvasPathBuilder>(f.Device.Get());

        auto strokeStyle = Make<CanvasStrokeStyle>();
        Matrix3x2 m{};
        ComPtr<ICanvasGeometry> g;
        CanvasGeometryRelation r;
        ComArray<CanvasTriangleVertices> t;
        float fl;
        boolean b;
        Rect rect;
        Vector2 pt;

        Assert::AreEqual(S_OK, canvasGeometry->Close());

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->CombineWith(otherCanvasGeometry.Get(), m, CanvasGeometryCombine::Union, &g));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->CombineWithUsingFlatteningTolerance(otherCanvasGeometry.Get(), m, CanvasGeometryCombine::Union, 0, &g));        

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->Stroke(0, &g));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->StrokeWithStrokeStyle(0, strokeStyle.Get(), &g));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->StrokeWithAllOptions(0, strokeStyle.Get(), m, 0, &g));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->Outline(&g));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->OutlineWithTransformAndFlatteningTolerance(m, 0, &g));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->Simplify(CanvasGeometrySimplification::Lines, &g));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->SimplifyWithTransformAndFlatteningTolerance(CanvasGeometrySimplification::Lines, m, 0, &g));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->Transform(m, &g));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->CompareWith(otherCanvasGeometry.Get(), &r));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->CompareWithUsingTransformAndFlatteningTolerance(otherCanvasGeometry.Get(), m, 0, &r));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeArea(&fl));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeAreaWithTransformAndFlatteningTolerance(m, 0, &fl));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputePathLength(&fl));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputePathLengthWithTransformAndFlatteningTolerance(m, 0, &fl));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputePointOnPath(0, &pt));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputePointOnPathWithTangent(0, &pt, &pt));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputePointOnPathWithTransformAndFlatteningToleranceAndTangent(0, m, 0, &pt, &pt));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->FillContainsPoint(Vector2{}, &b));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->FillContainsPointWithTransformAndFlatteningTolerance(Vector2{}, m, 0, &b));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeBounds(&rect));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeBoundsWithTransform(m, &rect));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeStrokeBounds(0, &rect));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeStrokeBoundsWithStrokeStyle(0, strokeStyle.Get(),&rect));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->ComputeStrokeBoundsWithAllOptions(0, strokeStyle.Get(), m, 0, &rect));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->StrokeContainsPoint(Vector2{}, 0, &b));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->StrokeContainsPointWithStrokeStyle(Vector2{}, 0, strokeStyle.Get(), &b));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->StrokeContainsPointWithAllOptions(Vector2{}, 0, strokeStyle.Get(), m, 0, &b));

        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->Tessellate(t.GetAddressOfSize(), t.GetAddressOfData()));
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->TessellateWithTransformAndFlatteningTolerance(m, 0, t.GetAddressOfSize(), t.GetAddressOfData()));

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(RO_E_CLOSED, canvasGeometry->get_Device(&retrievedDevice));
    }

    TEST_METHOD_EX(CanvasGeometry_DefaultFlatteningTolerance_CorrectValue)
    {
        auto canvasGeometryFactory = Make<CanvasGeometryFactory>();

        float flatteningTolerance;
        Assert::AreEqual(S_OK, canvasGeometryFactory->get_DefaultFlatteningTolerance(&flatteningTolerance));
        Assert::AreEqual(D2D1_DEFAULT_FLATTENING_TOLERANCE, flatteningTolerance);
    }

    TEST_METHOD_EX(CanvasGeometry_DefaultFlatteningTolerance_NullArg)
    {
        auto canvasGeometryFactory = Make<CanvasGeometryFactory>();

        Assert::AreEqual(E_INVALIDARG, canvasGeometryFactory->get_DefaultFlatteningTolerance(nullptr));
    }

    TEST_METHOD_EX(CanvasGeometry_get_Device)
    {
        Fixture f;

        auto canvasGeometry = f.Manager->Create(f.Device.Get(), Rect{});

        ComPtr<ICanvasDevice> device;
        Assert::AreEqual(S_OK, canvasGeometry->get_Device(&device));

        Assert::AreEqual(static_cast<ICanvasDevice*>(f.Device.Get()), device.Get());
    }

    TEST_METHOD_EX(CanvasGeometry_get_Device_Null)
    {
        Fixture f;

        auto canvasGeometry = f.Manager->Create(f.Device.Get(), Rect{});

        Assert::AreEqual(E_INVALIDARG, canvasGeometry->get_Device(nullptr));
    }
};