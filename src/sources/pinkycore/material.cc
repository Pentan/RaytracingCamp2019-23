//
//  material.cpp
//  PinkyCore
//
//  Created by SatoruNAKAJIMA on 2019/08/16.
//

#include <cmath>
#include "material.h"
#include "texture.h"
#include "pptypes.h"

using namespace PinkyPi;

namespace {
}

/////

void Sampler::basisFromNormal(const Vector3& n, Vector3* ou, Vector3* ov) {
    PPFloat s = (n.z < 0.0) ? -1.0 : 1.0;
    PPFloat a = -1.0 / (s + n.z);
    PPFloat b = n.x * n.y * a;
    ou->set(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
    ov->set(b, s + n.y * n.y * a, -n.y);
}

Sampler::SampleResult Sampler::sampleUniformHemisphere(const Vector3& n, Random& rng) {
    SampleResult res;
    Vector3 u, v;
    basisFromNormal(n, &u, &v);
    
    PPFloat z = rng.nextDoubleCC();
    PPFloat r = std::sqrt(std::max(0.0, 1.0 - z * z));
    PPFloat t = 2.0 * kPI * rng.nextDoubleCO();
    PPFloat x = r * std::cos(t);
    PPFloat y = r * std::sin(t);
    
    res.pdf =  1.0 / kPI;
    res.v = u * x + v * y + n * z;
    
    return res;
}

Sampler::SampleResult Sampler::sampleCosineWeightedHemisphere(const Vector3& n, Random& rng) {
    SampleResult res;
    Vector3 u, v;
    basisFromNormal(n, &u, &v);
    
    PPFloat a = rng.nextDoubleCO() * 2.0 - 1.0;
    PPFloat b = rng.nextDoubleCO() * 2.0 - 1.0;
    PPFloat x, y, z;
    if(a * a + b * b <= 0.0) {
        x = 0.0;
        y = 0.0;
        z = 1.0;
    } else {
        PPFloat t, r;
        if(std::abs(a) > std::abs(b)) {
            r = a;
            t = kPI * 0.25 * b / a;
        } else {
            r = b;
            t = kPI * 0.5 - kPI * 0.25 * a / b;
        }
        x = r * std::cos(t);
        y = r * std::sin(t);
        z = std::sqrt(std::max(0.0, 1.0 - r * r));
    }
    
    res.pdf = z / kPI;
    res.v = u * x + v * y + n * z;
    
    return res;
}

/////
Material::Material():
    assetId(-1),
    baseColorFactor(1.0, 1.0, 1.0),
    baseColorAlpha(1.0),
    metallicFactor(1.0),
    roughnessFactor(1.0),
    alphaMode(AlphaMode::kAlphaAsBlend), // spec default : OPAQUE
    alphaCutoff(0.5),
    doubleSided(true), // spec default : false
    ior(1.33),
    transmissionFactor(0.0),
    specularColorFactor(1.0, 1.0, 1.0),
    emissiveStrength(1.0)
{
    
}

Material::~Material() {
    
}

Color Material::evaluateThroughput(const Ray& iray, Ray* oray, const SurfaceInfo& surfinfo, Random& rng, EvalLog* log) const {
    Color ret;

    const Color albedo = evaluateAlbedoColor(surfinfo.uv0);
    const auto mr = evaluateMetallicRoughness(surfinfo.uv0);
    const auto metallic = mr.x;
    const auto roughness = mr.y;

    const auto alpha = roughness * roughness;

    PPFloat pdf = 1.0;
    BXDFId selected = BXDFId::kLambert;

#if 0
    if (rng.nextDoubleCO() < metallic) {
        // conductor
        pdf *= 1.0 / metallic;
        // TODO
        selected = BXDFId::kLambert;

    } else {
        // dielectric
        pdf *= 1.0 / (1.0 - metallic);

        const PPFloat f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);

        if (roughness == 0.0) {
            // full reflection
            selected = BXDFId::kSpecular;
        }
        else if (roughness == 1.0) {
            // full rough
            selected = BXDFId::kLambert;
        }
        else {
            // TODO
            selected = BXDFId::kLambert;
        }
    }
#endif

    log->selectedBxdfId = selected;
    switch (selected) {
        case BXDFId::kLambert:
        {
            log->selectedBxdfId = BXDFId::kLambert;
            log->bxdfType = BXDFType::kDiffuse;
            PPFloat s = (Vector3::dot(iray.direction, surfinfo.shadingNormal) > 0.0) ? -1.0 : 1.0;
            auto smpl = Sampler::sampleCosineWeightedHemisphere(surfinfo.shadingNormal * s, rng);
            Ray nray(surfinfo.position, smpl.v);
            PPFloat fx = evaluateBXDF(iray, nray, BXDFId::kLambert, surfinfo, log);
            *oray = nray;
            log->samplePdf = smpl.pdf;
            log->pdf = log->samplePdf * log->bxdfPdf;
            log->bxdfValue = fx;
            log->filterColor = albedo;
            ret = albedo * fx;
        }
            break;
        case BXDFId::kSpecular:
        {
            log->selectedBxdfId = BXDFId::kSpecular;
            log->bxdfType = BXDFType::kSpecular;
            PPFloat idotsn = Vector3::dot(iray.direction, surfinfo.shadingNormal);
            PPFloat s = (idotsn > 0.0) ? -1.0 : 1.0;
            Sampler::SampleResult smpl;
            smpl.v = iray.direction - surfinfo.shadingNormal * 2.0 * idotsn;
            smpl.pdf = 1.0;
            Ray nray(surfinfo.position, smpl.v);
            PPFloat fx = evaluateBXDF(iray, nray, BXDFId::kSpecular, surfinfo, log);
            *oray = nray;
            log->samplePdf = smpl.pdf;
            log->pdf = log->samplePdf * log->bxdfPdf;
            log->bxdfValue = fx;
            log->filterColor = Color::lerp(albedo, Color(1.0), metallic);
            ret = log->filterColor * fx;
        }
            break;
        case BXDFId::kTransmit:
            break;
        case BXDFId::kGGX:
            break;
        default:
            break;
    }
    return ret;
}

PPFloat Material::evaluateBXDF(const Ray& iray, const Ray& oray, BXDFId bxdfId, const SurfaceInfo& surfinfo, EvalLog* log) const {
    PPFloat ret = 0.0;
    Vector3 inv = iray.direction * -1.0;
    Vector3 outv = oray.direction;
    bool isCorrect = (Vector3::dot(inv, surfinfo.geometryNormal) * Vector3::dot(outv, surfinfo.geometryNormal)) > 0.0;

    switch (bxdfId) {
        case BXDFId::kLambert:
            ret = isCorrect ? (1.0 / kPI) : 0.0;
            log->bxdfPdf = 1.0;
            break;
        case BXDFId::kSpecular:
            ret = isCorrect ? (1.0 / std::abs(Vector3::dot(inv, surfinfo.shadingNormal))) : 0.0;
            log->bxdfPdf = 1.0;
            break;
        default:
            break;
    }
    return ret;
}

Color Material::evaluateEmissive(const Vector3* uv0) const {
    Color emit;
    emit = emissiveFactor * emissiveStrength;
    if(emissiveTexture.texture != nullptr) {
        const Vector3* uv = uv0 + std::max(0, emissiveTexture.texCoord);
        auto smpl = emissiveTexture.texture->sample(uv->x, uv->y, false);
        emit = Vector3::mul(emit, smpl.rgb);
    }
    return emit;
}

Color Material::evaluateAlbedoColor(const Vector3* uv0) const {
    Color alb;
    alb = baseColorFactor;
    if(baseColorTexture.texture != nullptr) {
        const Vector3* uv = uv0 + std::max(0, baseColorTexture.texCoord);
        auto smpl = baseColorTexture.texture->sample(uv->x, uv->y, true);
        alb = Vector3::mul(alb, smpl.rgb);
    }
    return alb;
}

Vector3 Material::evaluateMetallicRoughness(const Vector3* uv0) const {
    // x:metallic, y:roughness
    Vector3 mr;
    mr.x = metallicFactor;
    mr.y = roughnessFactor;
    if(metallicRoughnessTexture.texture != nullptr) {
        const Vector3* uv = uv0 + std::max(0, metallicRoughnessTexture.texCoord);
        auto smpl = metallicRoughnessTexture.texture->sample(uv->x, uv->y, false);
        mr = Vector3::mul(mr, smpl.rgb);
    }
    return mr;
}

Vector3 Material::evaluateNormal(const Vector3* uv0, const Vector3& nrmv, const Vector4& tanv) const {
    Vector3 retn = nrmv;
    if(normalTexture.texture != nullptr) {
        const Vector3* uv = uv0 + std::max(0, normalTexture.texCoord);
        auto smpl = normalTexture.texture->sample(uv->x, uv->y, false);
        Vector3 nmap(smpl.rgb.x, smpl.rgb.y, smpl.rgb.z);
        nmap = nmap * 2.0 - Vector3(1.0, 1.0, 1.0);
        Vector3 nv = Vector3::normalized(nrmv);
        Vector3 tv = Vector3::normalized(tanv.getXYZ());
        Vector3 btv = Vector3::cross(tv, nv) * tanv.w;
        
        retn.x = tv.x * nmap.x - btv.x * nmap.y + nv.x * nmap.z;
        retn.y = tv.y * nmap.x - btv.y * nmap.y + nv.y * nmap.z;
        retn.z = tv.z * nmap.x - btv.z * nmap.y + nv.z * nmap.z;
        retn.normalize();
    }
    return retn;
}
