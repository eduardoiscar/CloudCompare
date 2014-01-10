//##########################################################################
//#                                                                        #
//#                            CLOUDCOMPARE                                #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "ccSensor.h"

ccSensor::ccSensor(QString name)
	: ccHObject(name)
	, m_posBuffer(0)
	, m_activeIndex(0)
{
	m_rigidTransformation.toIdentity();
}

bool ccSensor::addPosition(ccGLMatrix& trans, double index)
{
	if (!m_posBuffer)
	{
		m_posBuffer = new ccIndexedTransformationBuffer();
		addChild(m_posBuffer);
	}

	bool sort = (!m_posBuffer->empty() && m_posBuffer->back().getIndex() > index);
	try
	{
		m_posBuffer->push_back(ccIndexedTransformation(trans,index));
	}
	catch(std::bad_alloc)
	{
		//not enough memory!
		return false;
	}

	if (sort)
		m_posBuffer->sort();

	return true;
}

void ccSensor::applyGLTransformation(const ccGLMatrix& trans)
{
	//we update the rigid transformation
	m_rigidTransformation = trans * m_rigidTransformation;
	ccHObject::applyGLTransformation(trans);
}

void ccSensor::getIndexBounds(double& minIndex, double& maxIndex) const
{
	if (m_posBuffer && !m_posBuffer->empty())
	{
		minIndex = m_posBuffer->front().getIndex();
		maxIndex = m_posBuffer->back().getIndex();
	}
	else
	{
		minIndex = maxIndex = 0;
	}
}

bool ccSensor::getAbsoluteTransformation(ccIndexedTransformation& trans, double index)
{
	trans.toIdentity();
	if (m_posBuffer)
		if (!m_posBuffer->getInterpolatedTransformation(index,trans))
			return false;

	trans *= m_rigidTransformation;

	return true;
}

bool ccSensor::toFile_MeOnly(QFile& out) const
{
	if (!ccHObject::toFile_MeOnly(out))
		return false;

	//rigid transformation (dataVersion>=34)
	if (!m_rigidTransformation.toFile(out))
		return WriteError();

	//active index (dataVersion>=34)
	if (out.write((const char*)&m_activeIndex,sizeof(double))<0)
		return WriteError();

	//we can't save the associated position buffer (as it may be shared by multiple sensors)
	//so instead we save it's unique ID (dataVersion>=34)
	//WARNING: the buffer must be saved in the same BIN file! (responsibility of the caller)
	uint32_t bufferUniqueID = (m_posBuffer ? (uint32_t)m_posBuffer->getUniqueID() : 0);
	if (out.write((const char*)&bufferUniqueID,4)<0)
		return WriteError();

	return true;
}

bool ccSensor::fromFile_MeOnly(QFile& in, short dataVersion, int flags)
{
	if (!ccHObject::fromFile_MeOnly(in, dataVersion, flags))
		return false;

	//serialization wasn't possible before v3.4!
	if (dataVersion < 34)
		return false;

	//rigid transformation (dataVersion>=34)
	if (!m_rigidTransformation.fromFile(in,dataVersion,flags))
		return ReadError();

	//active index (dataVersion>=34)
	if (in.read((char*)&m_activeIndex,sizeof(double))<0)
		return ReadError();

	//as the associated position buffer can't be saved directly (as it may be shared by multiple sensors)
	//we only store its unique ID (dataVersion>=34) --> we hope we will find it at loading time (i.e. this
	//is the responsibility of the caller to make sure that all dependencies are saved together)
	uint32_t bufferUniqueID = 0;
	if (in.read((char*)&bufferUniqueID,4)<0)
		return ReadError();
	//[DIRTY] WARNING: temporarily, we set the vertices unique ID in the 'm_posBuffer' pointer!!!
	*(uint32_t*)(&m_posBuffer) = bufferUniqueID;

	return true;
}
