#include "common.h"
#include "snowmaterialmanager.h"
#include <mitsuba/render/shape.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/subsurface.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/fstream.h>

//#define DEBUG_DIFF_PROF

MTS_NAMESPACE_BEGIN

void SnowMaterialManager::replaceMaterial(Shape *shape, SceneContext *context) {
        // get currently selected material
        BSDF *bsdfOld = shape->getBSDF();
        Subsurface *subsurfaceOld = shape->getSubsurface();

        // try to find shape in store 
        ShapeMap::iterator it = snowShapes.find(shape);
        /* If not found, add new.
         */
        if (it == snowShapes.end()) {
            ShapeEntry newEntry;

            newEntry.originalBSDF = bsdfOld;
            if (bsdfOld != NULL)
                bsdfOld->incRef();

            newEntry.originalSubsurface = subsurfaceOld;
            if (subsurfaceOld != NULL)
                subsurfaceOld->incRef();

            shape->incRef();
            snowShapes[shape] = newEntry;
        }

        PluginManager *pluginManager = PluginManager::getInstance();
        SnowProperties &snow = context->snow;
        ESurfaceRenderMode surfaceMode = context->snowRenderSettings.surfaceRenderMode;
        ESubSurfaceRenderMode subsurfaceMode = context->snowRenderSettings.subsurfaceRenderMode;

        // BSDF setup
        Properties bsdfProps;
        bsdfProps.setFloat("g", snow.g);
        bsdfProps.setFloat("eta", snow.ior); // ToDo: eta is actually the relative IOR (no prob w/ air)
 
        BSDF *bsdf = NULL;
        SnowRenderSettings &srs = context->snowRenderSettings;
        bool hasBSDF = true;

        if (surfaceMode == ENoSurface) {
            hasBSDF = false;
        } else if (surfaceMode == EWiscombeWarrenAlbedo) {
            bsdfProps.setPluginName("wiscombe");
            bsdfProps.setSpectrum("sigmaT", snow.sigmaT);
            bsdfProps.setFloat("depth",srs.wiscombeDepth);
            bsdfProps.setSpectrum("singleScatteringAlbedo", snow.singleScatteringAlbedo);
        } else if (surfaceMode == EWiscombeWarrenBRDF) {
            bsdfProps.setPluginName("wiscombe");
            bsdfProps.setFloat("depth", srs.wiscombeDepth);
            bsdfProps.setSpectrum("singleScatteringAlbedo", snow.singleScatteringAlbedo);
        } else if (surfaceMode == EHanrahanKruegerBRDF) {
            //bsdfProps.setPluginName("hanrahankrueger");
            bsdfProps.setPluginName("hk");
            bsdfProps.setSpectrum("sigmaA", snow.sigmaA);
            bsdfProps.setSpectrum("sigmaS", snow.sigmaS);
            bsdfProps.setFloat("thickness", 0.01);
            //bsdfProps.setSpectrum("ssFactor", Spectrum(srs.hkSingleScatteringFactor));
            //bsdfProps.setSpectrum("drFactor", Spectrum(srs.hkMultipleScatteringFactor));
            //bsdfProps.setBoolean("diffuseReflectance", srs.hkUseMultipleScattering);
        } else if (surfaceMode == EMicrofacetBRDF) {
            bsdfProps.setPluginName("roughglass");
            bsdfProps.setFloat("alpha", 0.9f);
            bsdfProps.setFloat("intIOR", snow.ior);
            bsdfProps.setString("distribution", "ggx");
        }

        if (hasBSDF)
            bsdf = static_cast<BSDF *> (pluginManager->createObject(
                BSDF::m_theClass, bsdfProps));

        // Subsurface setup
        Subsurface *subsurface = NULL;
        Properties subsurfaceProps;
        subsurfaceProps.setFloat("g", snow.g);
        subsurfaceProps.setFloat("eta", snow.ior); // ToDo: eta is actually the relative IOR (no prob w/ air)

        if (subsurfaceMode == ENoSubSurface) {
            subsurface = NULL; 
        } else if (subsurfaceMode == EJensenDipoleBSSRDF) {
            subsurfaceProps.setPluginName("dipole");
            subsurfaceProps.setSpectrum("sigmaA", snow.sigmaA);
            subsurfaceProps.setSpectrum("sigmaS", snow.sigmaS);
            subsurfaceProps.setSpectrum("ssFactor", Spectrum(srs.dipoleDensityFactor));
            subsurfaceProps.setFloat("sampleMultiplier", srs.dipoleSampleFactor);
            subsurfaceProps.setBoolean("singleScattering", srs.dipoleUseSingleScattering);
            subsurfaceProps.setBoolean("useMartelliD", srs.dipoleMartelliDC);
            subsurfaceProps.setBoolean("useTexture", srs.dipoleTexture);
            subsurfaceProps.setBoolean("dumpIrrtree", srs.dipoleDumpIrrtree);
            subsurfaceProps.setBoolean("hasRoughSurface", srs.dipoleHasRoughSurface);
            subsurfaceProps.setString("dumpIrrtreePath", srs.dipoleDumpIrrtreePath);
            if (srs.dipoleLutPredefineRmax) {
                subsurfaceProps.setFloat("lutRmax", srs.dipoleLutRmax);
            } else {
                subsurfaceProps.setInteger("mcIterations", srs.dipoleLutMCIterations);
            }
            if (srs.dipoleTexture) {
                subsurfaceProps.setString("zrFilename", srs.dipoleZrTexture);
                subsurfaceProps.setString("sigmaTrFilename", srs.dipoleSigmaTrTexture);
                subsurfaceProps.setFloat("texUScaling", srs.dipoleTextureUScaling);
                subsurfaceProps.setFloat("texVScaling", srs.dipoleTextureVScaling);
            }
            subsurfaceProps.setBoolean("useLookUpTable", srs.dipoleUseLut);
            subsurfaceProps.setFloat("lutResolution", srs.dipoleLutResolution);
            subsurface = static_cast<Subsurface *> (pluginManager->createObject(
                Subsurface::m_theClass, subsurfaceProps));
        } else if (subsurfaceMode == EJensenMultipoleBSSRDF) {
            subsurfaceProps.setPluginName("multipole");
            subsurfaceProps.setSpectrum("sigmaA", snow.sigmaA);
            subsurfaceProps.setSpectrum("sigmaS", snow.sigmaS);
            subsurfaceProps.setSpectrum("ssFactor", Spectrum(srs.multipoleDensityFactor));
            subsurfaceProps.setFloat("sampleMultiplier", srs.multipoleSampleFactor);
            subsurfaceProps.setBoolean("singleScattering", srs.dipoleUseSingleScattering);
            subsurfaceProps.setFloat("slabThickness", srs.multipoleSlabThickness);
            subsurfaceProps.setInteger("extraDipoles", srs.multipoleExtraDipoles);
            subsurfaceProps.setBoolean("useMartelliD", srs.multipoleMartelliDC);
            subsurfaceProps.setBoolean("useLookUpTable", srs.multipoleUseLut);
            subsurfaceProps.setFloat("lutResolution", srs.multipoleLutResolution);
            if (srs.multipoleLutPredefineRmax) {
                subsurfaceProps.setFloat("lutRmax", srs.multipoleLutRmax);
            } else {
                subsurfaceProps.setInteger("mcIterations", srs.multipoleLutMCIterations);
            }
            subsurface = static_cast<Subsurface *> (pluginManager->createObject(
                Subsurface::m_theClass, subsurfaceProps));
        } else if (subsurfaceMode == EJakobADipoleBSSRDF) {
            subsurfaceProps.setPluginName("adipole");
            subsurfaceProps.setSpectrum("sigmaA", snow.sigmaA);
            subsurfaceProps.setSpectrum("sigmaS", snow.sigmaS);
            subsurfaceProps.setSpectrum("ssFactor", Spectrum(srs.adipoleDensityFactor));
            subsurfaceProps.setFloat("sampleMultiplier", srs.adipoleSampleFactor);
            QString D = QString::fromStdString(srs.adipoleD);
            if (D.trimmed().length() == 0)
                subsurfaceProps.setString("D", getFlakeDistribution());
            else
                subsurfaceProps.setString("D", srs.adipoleD);
            subsurfaceProps.setFloat("sigmaTn", srs.adipoleSigmaTn);
            subsurface = static_cast<Subsurface *> (pluginManager->createObject(
                Subsurface::m_theClass, subsurfaceProps));
        }

        /* initialize new materials */
        if (bsdf) {
            bsdf->setParent(shape);
            bsdf->configure();
            bsdf->incRef();
        }
        if (subsurface) {
            subsurface->setParent(shape);
            subsurface->configure();
            subsurface->incRef();
            // if a subsurface material has been selected, inform the scene about it
            context->scene->addSubsurface(subsurface);
        }

        shape->setBSDF(bsdf);
        shape->setSubsurface(subsurface);
        // allow the shape to react to this changes
        shape->configure();

        /* if the subsurface integrator previously used (if any) is not
         * needed by other shapes, we can remove it for now.
         */
        if (subsurfaceOld != NULL ) {
            context->scene->removeSubsurface(subsurfaceOld);
        }

        setMadeOfSnow(shape, true);
        std::string bsdfName = (bsdf == NULL) ? "None" : bsdf->getClass()->getName();
        std::string subsurfaceName = (subsurface == NULL) ? "None" : subsurface->getClass()->getName();
        std::cerr << "[Snow Material Manager] Replaced material of shape \"" << shape->getName() << "\"" << std::endl
                  << "\tnew BSDF: " << bsdfName << std::endl
                  << "\tnew Subsurface: " << subsurfaceName << std::endl;
}

std::string SnowMaterialManager::getFlakeDistribution() {
    // My (Tom) calculations (indefinite Matrizen, geht nicht):
    /* clamped cosin^20 flake distribution */
    // return "0.01314, -0.00014, 0.00061, -0.00014, 0.01295, -0.00018, 0.00061, -0.00018, -0.07397";
    /* sine^20 flake distribution */
    //return "1.6307, -0.00049, 0.00069, -0.00049, 1.63148, 0.00001, 0.00067, 0.00002, 2.12596";

    // Wenzels Berechnungen (definite Matrizen, notwendig)
    /* clamped cosin^20 flake distribution,hier ist D(w) = abs(dot(w, [0, 0, 1]))^20.000000  */
    //return "0.043496, 4.0726e-10, -1.1039e-10, 4.0726e-10, 0.043496, 1.1632e-09, -1.1039e-10, 1.1632e-09, 0.91301";
    /* sine^20 flake distribution, hier ist D(w) = (1-dot(w, [0, 0, 1])^2)^10.000000 */
    return "0.47827, 7.5057e-09, -4.313e-10, 7.5057e-09, 0.47827, 2.5069e-10, -4.313e-10, 2.5069e-10, 0.043454";
}

void SnowMaterialManager::resetMaterial(Shape *shape, SceneContext *context) {
        ShapeMap::iterator it = snowShapes.find(shape);
        if (it == snowShapes.end()) {
            SLog(EWarn, "Did not find requested shape to reset material.");
            return;
        }

        setMadeOfSnow(shape, false);
        ShapeEntry &e = it->second;

        // if found, use materials
        BSDF *bsdf = e.originalBSDF;
        if (bsdf != NULL) {
            bsdf->setParent(shape);
            bsdf->configure();
        }
        shape->setBSDF( bsdf );

        Subsurface *old_ss = shape->getSubsurface();
        Subsurface *subsurface = e.originalSubsurface;

        if (subsurface != NULL) {
            subsurface->setParent(shape);
            subsurface->configure();
            context->scene->addSubsurface(subsurface);
        }
        if (old_ss != NULL)
            context->scene->removeSubsurface(old_ss);

        shape->setSubsurface(subsurface);
        // allow the shape to react to this changes
        shape->configure();

        std::cerr << "[Snow Material Manager] Reset material on shape " << shape->getName() << std::endl;
}

bool SnowMaterialManager::isMadeOfSnow(const Shape * shape) const {
    ShapeMap::const_iterator it = snowShapes.find(shape);
    if (it != snowShapes.end())
        return it->second.madeOfSnow;
    else
        return false;
}

void SnowMaterialManager::removeShape(Shape *shape) {
    ShapeMap::iterator it = snowShapes.find(shape);
    if (it != snowShapes.end())
        snowShapes.erase(it);
}

void SnowMaterialManager::setMadeOfSnow(Shape *shape, bool snow) {
    ShapeMap::iterator it = snowShapes.find(shape);
    if (it == snowShapes.end())
        return;

    it->second.madeOfSnow = snow;
}

std::string SnowMaterialManager::toString() {
		std::ostringstream oss;
		oss << "SnowMaterialManager[" << std::endl;

        for (ShapeMap::iterator it = snowShapes.begin(); it != snowShapes.end(); it++) {
            const Shape *s = it->first;
            ShapeEntry &entry = it->second;
            if (entry.madeOfSnow && s != NULL) {
                const BSDF* bsdf = s->getBSDF();
                const Subsurface* subsurface = s->getSubsurface();

                oss << "  " << s->getName() << ":" << std::endl
                << "    BSDF: " << std::endl << (bsdf == NULL ? "None" : indent(bsdf->toString(), 3)) << std::endl
                << "    Subsurface: " << std::endl << (subsurface == NULL ? "None" : indent(subsurface->toString(), 3)) << std::endl;
            }
        }
		oss	<< "]";
		return oss.str();
}

std::pair< ref<Bitmap>, Float > SnowMaterialManager::getCachedDiffusionProfile() const {
    return std::make_pair(diffusionProfileCache, diffusionProfileRmax);
}

bool SnowMaterialManager::hasCachedDiffusionProfile() const {
    return diffusionProfileCache.get() != NULL;
}

void SnowMaterialManager::refreshDiffusionProfile(const SceneContext *context) {
    typedef SubsurfaceMaterialManager::LUTType LUTType;

    const Float errThreshold = 0.01f;
    const Float lutResolution = 0.001f;
    const SnowProperties &sp = context->snow;
    const bool rMaxPredefined = context->snowRenderSettings.shahPredefineRmax;
    const Float predefinedRmax = context->snowRenderSettings.shahRmax;

    const Spectrum sigmaSPrime = sp.sigmaS * (1 - sp.g);
    const Spectrum sigmaTPrime = sigmaSPrime + sp.sigmaA;

    /* extinction coefficient */
    const Spectrum sigmaT = sp.sigmaA + sp.sigmaS;

    /* Effective transport extinction coefficient */
    const Spectrum sigmaTr = (sp.sigmaA * sigmaTPrime * 3.0f).sqrt();

    /* Reduced albedo */
    const Spectrum alphaPrime = sigmaSPrime / sigmaTPrime;

    /* Mean-free path (avg. distance traveled through the medium) */
    const Spectrum mfp = Spectrum(1.0f) / sigmaTPrime;

    Float Fdr;
    if (sp.ior > 1) {
        /* Average reflectance due to mismatched indices of refraction
           at the boundary - [Groenhuis et al. 1983]*/
        Fdr = -1.440f / (sp.ior * sp.ior) + 0.710f / sp.ior
            + 0.668f + 0.0636f * sp.ior;
    } else {
        /* Average reflectance due to mismatched indices of refraction
           at the boundary - [Egan et al. 1973]*/
        Fdr = -0.4399f + 0.7099f / sp.ior - 0.3319f / (sp.ior * sp.ior)
            + 0.0636f / (sp.ior * sp.ior * sp.ior);
    }

    /* Average transmittance at the boundary */
    Float Fdt = 1.0f - Fdr;

    if (sp.ior == 1.0f) {
        Fdr = (Float) 0.0f;
        Fdt = (Float) 1.0f;
    }

    /* Approximate dipole boundary condition term */
    const Float A = (1 + Fdr) / Fdt;

    /* Distance of the dipole point sources to the surface */
    const Spectrum zr = mfp;
    const Spectrum zv = mfp * (1.0f + 4.0f/3.0f * A);

    const Spectrum invSigmaTr = Spectrum(1.0f) / sigmaTr;
    const Float inv4Pi = 1.0f / (4 * M_PI);

    Float rMax = 0.0f;

    if (!rMaxPredefined) {
        ref<Random> random = new Random();

        /* Find Rd for the whole area by monte carlo integration. The
         * sampling area is calculated from the max. mean free path.
         * A square area around with edge length 2 * maxMFP is used
         * for this. Hene, the sampling area is 4 * maxMFP * maxMFP. */
        const int numSamples = context->snowRenderSettings.shahMCIterations;
        Spectrum Rd_A = Spectrum(0.0f);
        for (int n = 0; n < numSamples; ++n) {
            /* do importance sampling by choosing samples distributed
             * with sigmaTr^2 * e^(-sigmaTr * r). */
            Spectrum r = invSigmaTr * -std::log( random->nextFloat() );
            Rd_A += getRd(r, sigmaTr, zv, zr);
        }
        Float Area = 4 * invSigmaTr.max() * invSigmaTr.max();
        Rd_A = Area * Rd_A * alphaPrime * inv4Pi / (Float)(numSamples - 1);
        SLog(EDebug, "After %i MC integration iterations, Rd seems to be %s", numSamples, Rd_A.toString().c_str());

        /* Since we now have Rd integrated over the whole surface we can find a valid rmax
         * for the given threshold. */
        const Float step = lutResolution;
        Spectrum err(std::numeric_limits<Float>::max());
        while (err.max() > errThreshold) {
            rMax += step;
            /* Again, do MC integration, but with r clamped at rmax. */
            Spectrum Rd_APrime(0.0f);
            for (int n = 0; n < numSamples; ++n) {
                /* do importance sampling by choosing samples distributed
                 * with sigmaTr^2 * e^(-sigmaTr * r). */
                Spectrum r = invSigmaTr * -std::log( random->nextFloat() );
                // clamp samples to rMax
                for (int s=0; s<SPECTRUM_SAMPLES; ++s) {
                    r[s] = std::min(rMax, r[s]);
                }
                Rd_APrime += getRd(r, sigmaTr, zv, zr);
            }
            Float APrime = 4 * rMax * rMax;
            Rd_APrime = APrime * Rd_APrime * alphaPrime * inv4Pi / (Float)(numSamples - 1);
            err = (Rd_A - Rd_APrime) / Rd_A;
        }
        SLog(EDebug, "Maximum distance for sampling surface is %f with an error of %f", rMax, errThreshold);
    } else {
        rMax = predefinedRmax;
    }

    /* Create the actual look-up-table */
    const int numEntries = (int) (rMax / lutResolution) + 1;
    std::vector<Spectrum> diffusionProfile(numEntries);
    for (int i=0; i<numEntries; ++i) {
        Spectrum r = Spectrum(i * lutResolution);
        diffusionProfile.at(i) = getRd(r, sigmaTr, zv, zr);
    }
    SLog(EDebug, "Created Rd diffusion profile with %i entries.", numEntries);

    /* Create the diffuson profile bitmap */
    diffusionProfileRmax = rMax;
    bool useHDR = true;

    if (useHDR) {
        diffusionProfileCache = new Bitmap(numEntries, 1, 128);
        float *data = diffusionProfileCache->getFloatData();
        for (int i = 0; i < numEntries; ++i) {
            *data++ = diffusionProfile[i][0];
            *data++ = diffusionProfile[i][1];
            *data++ = diffusionProfile[i][2];
            *data++ = 1.0f;
        }
#ifdef DEBUG_DIFF_PROF
        data = diffusionProfileCache->getFloatData();
        for (int i = 0; i < numEntries * 4; ++i) {
            std::cerr << *data++ << " ";
        }
        std::cerr << std::endl;

        diffusionProfileCache->save(
            Bitmap::EEXR,
            new FileStream("img-diffprof.exr", FileStream::ETruncWrite));
#endif
    } else {
        diffusionProfileCache = new Bitmap(numEntries, 1, 32);
        Float maxRd = diffusionProfile[0].max();
        const Float scale = 255.0f / maxRd;
        const Float exposure = context->snowRenderSettings.shahExposure;
        unsigned char *data = diffusionProfileCache->getData();
        for (int i = 0; i < numEntries; ++i) {
            *data++ = std::max( (unsigned int)1, (unsigned int) (diffusionProfile[i][0] * scale + 0.5));
            *data++ = std::max( (unsigned int)1, (unsigned int) (diffusionProfile[i][1] * scale + 0.5));
            *data++ = std::max( (unsigned int)1, (unsigned int) (diffusionProfile[i][2] * scale + 0.5));
            //*data++ = int(255.0f * (1.0f - exp(diffusionProfile[i][0] * exposure)));
            //*data++ = int(255.0f * (1.0f - exp(diffusionProfile[i][1] * exposure)));
            //*data++ = int(255.0f * (1.0f - exp(diffusionProfile[i][2] * exposure)));
            *data++ = 255;
        }
#ifdef DEBUG_DIFF_PROF
        data = diffusionProfileCache->getData();
        for (int i = 0; i < numEntries * 4; ++i) {
            std::cerr << (int) (*data++) << " ";
        }
        std::cerr << std::endl;

        diffusionProfileCache->save(
            Bitmap::EPNG,
            new FileStream("img-diffprof.png", FileStream::ETruncWrite));
#endif
    }
}

/// Calculate Rd based on all dipoles and the requested distance
Spectrum SnowMaterialManager::getRd(const Spectrum &r, const Spectrum &sigmaTr,
        const Spectrum &zv, const Spectrum &zr) { 
    //Float dist = std::max((p - sample.p).lengthSquared(), zrMinSq); 
    const Spectrum rSqr = r * r;
    const Spectrum one(1.0f);
    const Spectrum negSigmaTr = sigmaTr * (-1.0f);

    /* Distance to the real source */
    Spectrum dr = (rSqr + zr*zr).sqrt();
    /* Distance to the image point source */
    Spectrum dv = (rSqr + zv*zv).sqrt();

    Spectrum C1 = zr * (sigmaTr + one / dr);
    Spectrum C2 = zv * (sigmaTr + one / dv);

    /* Do not include the reduced albedo - will be canceled out later */
    Spectrum Rd = Spectrum(0.25f * INV_PI) *
         (C1 * ((negSigmaTr * dr).exp()) / (dr * dr)
        + C2 * ((negSigmaTr * dv).exp()) / (dv * dv));

    return Rd;
}

MTS_NAMESPACE_END