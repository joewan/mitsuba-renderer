/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2011 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/core/properties.h>
#include <mitsuba/render/subsurface.h>
#include <mitsuba/render/shape.h>

MTS_NAMESPACE_BEGIN

Subsurface::Subsurface(const Properties &props)
 : NetworkedObject(props) {
	/* Skim milk data from "A Practical Model for Subsurface scattering" (Jensen et al.) */
	Spectrum defaultSigmaS, defaultSigmaA;

	defaultSigmaA.fromLinearRGB(0.0014f, 0.0025f, 0.0142f);
	defaultSigmaS.fromLinearRGB(0.7f, 1.22f, 1.9f);

	if (props.hasProperty("sizeMultiplier"))
		Log(EError, "Deprecation error: the parameter sizeMultiplier"
			" has been renamed to densityMultiplier");

	m_densityMultiplier = props.getFloat("densityMultiplier", 1);
	/* Scattering coefficient */
	m_sigmaS = props.getSpectrum("sigmaS", defaultSigmaS);
	/* Absorption coefficient */
	m_sigmaA = props.getSpectrum("sigmaA", defaultSigmaA);
	/* Refractive index of the object */
	m_eta = props.getFloat("eta", 1.5f);
		
	m_sigmaS *= m_densityMultiplier;
	m_sigmaA *= m_densityMultiplier;
	m_sigmaT = m_sigmaS + m_sigmaA;
}

Subsurface::Subsurface(Stream *stream, InstanceManager *manager) :
	NetworkedObject(stream, manager) {
	m_sigmaS = Spectrum(stream);
	m_sigmaA = Spectrum(stream);
	m_eta = stream->readFloat();
	m_densityMultiplier = stream->readFloat();
	size_t shapeCount = stream->readSize();

	for (size_t i=0; i<shapeCount; ++i) {
		Shape *shape = static_cast<Shape *>(manager->getInstance(stream));
		m_shapes.push_back(shape);
	}
	m_sigmaT = m_sigmaS + m_sigmaA;
}

Subsurface::~Subsurface() {
}
	
void Subsurface::cancel() {
}

void Subsurface::setParent(ConfigurableObject *parent) {
	if (parent->getClass()->derivesFrom(MTS_CLASS(Shape))) {
		Shape *shape = static_cast<Shape *>(parent);
		if (shape->isCompound())
			return;
		if (std::find(m_shapes.begin(), m_shapes.end(), shape) == m_shapes.end()) {
			m_shapes.push_back(shape);
			m_configured = false;
		}
	} else {
		Log(EError, "IsotropicDipole: Invalid child node!");
	}
}

void Subsurface::serialize(Stream *stream, InstanceManager *manager) const {
	NetworkedObject::serialize(stream, manager);

	m_sigmaS.serialize(stream);
	m_sigmaA.serialize(stream);
	stream->writeFloat(m_eta);
	stream->writeFloat(m_densityMultiplier);
	stream->writeSize(m_shapes.size());
	for (unsigned int i=0; i<m_shapes.size(); ++i)
		manager->serialize(stream, m_shapes[i]);
}

ref<SubsurfaceMaterialManager> SubsurfaceMaterialManager::m_instance = new SubsurfaceMaterialManager();

bool SubsurfaceMaterialManager::hasLUT(const std::string &hash) const {
   return m_lutRecords.find(hash) != m_lutRecords.end();
} 

void SubsurfaceMaterialManager::addLUT(const std::string &hash, const LUTRecord &lutRec) {
    if (!hasLUT(hash))
        m_lutRecords[hash] = LUTRecord(lutRec.lut, lutRec.resolution);
}

SubsurfaceMaterialManager::LUTRecord SubsurfaceMaterialManager::getLUT(const std::string &hash) const {
    return m_lutRecords.find(hash)->second;
}

std::string SubsurfaceMaterialManager::getMultipoleLUTHashR(Float resolution, Float errorThreshold,
        const Spectrum &sigmaTr, const Spectrum &alphaPrime, int numExtraDipoles,
        const std::vector<Spectrum> &zrList, const std::vector<Spectrum> &zvList) const {
    std::ostringstream oss;
    // set precision to get around floating point rounding issues
    oss.precision(5);
    oss << "multipole" << resolution << "," << errorThreshold << "," << sigmaTr.toString() << ","
        << alphaPrime.toString() << "," << numExtraDipoles;

    for (std::vector<Spectrum>::const_iterator it=zrList.begin(); it!=zrList.end(); ++it)
        oss << it->toString();
    for (std::vector<Spectrum>::const_iterator it=zvList.begin(); it!=zvList.end(); ++it)
        oss << it->toString();

    return oss.str();
}

std::string SubsurfaceMaterialManager::getMultipoleLUTHashT(Float resolution, Float errorThreshold,
        const Spectrum &sigmaTr, const Spectrum &alphaPrime, int numExtraDipoles,
        const std::vector<Spectrum> &zrList, const std::vector<Spectrum> &zvList, Float d) const {
    std::ostringstream oss;
    oss << getMultipoleLUTHashR(resolution, errorThreshold, sigmaTr, alphaPrime, numExtraDipoles,
            zrList, zvList);
    oss << ",d=" << d;
    return oss.str();
}

std::string SubsurfaceMaterialManager::getDipoleLUTHash(Float resolution, Float errorThreshold,
    const Spectrum &sigmaTr, const Spectrum &alphaPrime, 
    const Spectrum &zr, const Spectrum &zv) const {
    std::ostringstream oss;
    // set precision to get around floating point rounding issues
    oss.precision(5);
    oss << "multipole" << resolution << "," << errorThreshold << "," << sigmaTr.toString() << ","
        << alphaPrime.toString() << "," << zr.toString() << "," << zv.toString();
    return oss.str();
}

SubsurfaceMaterialManager::~SubsurfaceMaterialManager() { }

MTS_IMPLEMENT_CLASS(SubsurfaceMaterialManager, false, Object)
MTS_IMPLEMENT_CLASS(Subsurface, true, NetworkedObject)
MTS_NAMESPACE_END
