/*=========================================================================

  Program: DICOM for VTK

  Copyright (c) 2012-2013 David Gobbi
  All rights reserved.
  See Copyright.txt or http://www.cognitive-antics.net/bsd3.txt for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkDICOMSCGenerator.h"
#include "vtkDICOMMetaData.h"
#include "vtkDICOMSequence.h"
#include "vtkDICOMItem.h"
#include "vtkDICOMTagPath.h"

#include "vtkObjectFactory.h"
#include "vtkDataSetAttributes.h"
#include "vtkInformation.h"

vtkStandardNewMacro(vtkDICOMSCGenerator);

//----------------------------------------------------------------------------
vtkDICOMSCGenerator::vtkDICOMSCGenerator()
{
}

//----------------------------------------------------------------------------
vtkDICOMSCGenerator::~vtkDICOMSCGenerator()
{
}

//----------------------------------------------------------------------------
void vtkDICOMSCGenerator::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateSCMultiFrameImageModule(
  vtkDICOMMetaData *meta, vtkInformation *info)
{
  // The BurnedInAnnotation attribute is mandatory
  std::string bia;
  if (this->MetaData)
    {
    bia = this->MetaData->GetAttributeValue(
      DC::BurnedInAnnotation).AsString();
    }
  if (bia != "YES")
    {
    bia = "NO";
    }
  meta->SetAttributeValue(DC::BurnedInAnnotation, bia);

  // These are mandatory, and must be set to these values
  if (meta->GetAttributeValue(DC::BitsStored).AsInt() != 1)
    {
    meta->SetAttributeValue(DC::PresentationLUTShape, "IDENTITY");
    meta->SetAttributeValue(DC::RescaleIntercept, 0.0);
    meta->SetAttributeValue(DC::RescaleSlope, 1.0);
    meta->SetAttributeValue(DC::RescaleType, "US");
    }

  // get dimensions of the data set: x,y,z,t,v
  int nframes = 1;
  int dims[5];
  double spacing[5];
  this->ComputeDimensions(info, &nframes, dims, spacing);

  // set multi-frame information
  meta->SetAttributeValue(DC::NumberOfFrames, nframes);

  // build the FrameIncrementPointer and the vectors
  vtkDICOMTag pointers[2];
  int npointers = 0;
  double *zvector = new double[nframes];
  double *tvector = new double[nframes];

  int vdim = (dims[4] > 0 ? dims[4] : 1);
  int tdim = (dims[3] > 0 ? dims[3] : 1);
  for (int f = 0; f < nframes; f++)
    {
    int t = (f / vdim) % tdim;
    int z = f / (vdim * tdim);
    zvector[f] = z*spacing[2];
    tvector[f] = (t == 0 ? 0.0 : spacing[3]);
    }

  if (dims[3] > 0 || (dims[2] == 0 && nframes == 1))
    {
    pointers[npointers++] = DC::FrameTimeVector;
    meta->SetAttributeValue(
      DC::FrameTimeVector,
      vtkDICOMValue(vtkDICOMVR::DS, tvector, tvector+nframes));
    }
  if (dims[2] > 0)
    {
    pointers[npointers++] = DC::SliceLocationVector;
    meta->SetAttributeValue(DC::SliceLocationVector,
      vtkDICOMValue(vtkDICOMVR::DS, zvector, zvector+nframes));
    }

  meta->SetAttributeValue(
    DC::FrameIncrementPointer,
    vtkDICOMValue(vtkDICOMVR::AT, pointers, pointers+npointers));

  meta->RemoveAttribute(DC::PixelAspectRatio);
  meta->SetAttributeValue(
    DC::PixelSpacing,
    vtkDICOMValue(vtkDICOMVR::DS, spacing, spacing+2));

  delete [] zvector;
  delete [] tvector;

  // optional and conditional: direct copy of values with no checks
  static const DC::EnumType optional[] = {
    DC::RecognizableVisualFeatures,
    DC::Illumination,
    DC::ReflectedAmbientLight,
    DC::NominalScannedPixelSpacing, // 1C (mandatory if ConversionType is DF)
    DC::PixelSpacingCalibrationType,
    DC::PixelSpacingCalibrationDescription, // 1C
    DC::DigitizingDeviceTransportDirection,
    DC::RotationOfScannedFilm,
    DC::ItemDelimitationItem
  };

  if (this->MetaData)
    {
    for (int i = 0; optional[i] != DC::ItemDelimitationItem; i++)
      {
      meta->SetAttributeValue(optional[i],
        this->MetaData->GetAttributeValue(optional[i]));
      }
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateSCImageModule(vtkDICOMMetaData *meta)
{
  // These are not set, because AspectRatio is used instead  
  // DC::PixelSpacing, // 1C
  // DC::PixelSpacingCalibrationType,
  // DC::PixelSpacingCalibrationDescription, // 1C

  // optional and conditional: direct copy of values with no checks
  static const DC::EnumType optional[] = {
    DC::DateOfSecondaryCapture,
    DC::TimeOfSecondaryCapture,
    DC::NominalScannedPixelSpacing,
    DC::DocumentClassCodeSequence,
    DC::ItemDelimitationItem
  };

  if (this->MetaData)
    {
    for (int i = 0; optional[i] != DC::ItemDelimitationItem; i++)
      {
      meta->SetAttributeValue(optional[i],
        this->MetaData->GetAttributeValue(optional[i]));
      }
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateSCEquipmentModule(vtkDICOMMetaData *meta)
{
  // ConversionType is a mandatory attribute
  std::string ct;
  if (this->MetaData)
    {
    ct = this->MetaData->GetAttributeValue(DC::ConversionType).AsString();
    }
  if (ct == "")
    {
    ct = "WSD"; // workstation
    }
  meta->SetAttributeValue(DC::ConversionType, ct);

  // optional and conditional: direct copy of values with no checks
  static const DC::EnumType optional[] = {
    DC::Modality,
    DC::SecondaryCaptureDeviceID,
    DC::SecondaryCaptureDeviceManufacturer,
    DC::SecondaryCaptureDeviceManufacturerModelName,
    DC::SecondaryCaptureDeviceSoftwareVersions,
    DC::VideoImageFormatAcquired,
    DC::DigitalImageFormatAcquired,
    DC::ItemDelimitationItem
  };

  if (this->MetaData)
    {
    for (int i = 0; optional[i] != DC::ItemDelimitationItem; i++)
      {
      meta->SetAttributeValue(optional[i],
        this->MetaData->GetAttributeValue(optional[i]));
      }
    }

  // Modality is optional for Secondary Capture
  std::string m = meta->GetAttributeValue(DC::Modality).AsString();
  if (m == "" || m == "OT")
    {
    meta->RemoveAttribute(DC::Modality);
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateSCInstance(
  vtkInformation *info, vtkDICOMMetaData *meta)
{
  // get the scalar information
  vtkInformation *scalarInfo = vtkDataObject::GetActiveFieldInformation(
    info, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::SCALARS);

  int scalarType = scalarInfo->Get(vtkDataObject::FIELD_ARRAY_TYPE());
  int numComponents = scalarInfo->Get(
    vtkDataObject::FIELD_NUMBER_OF_COMPONENTS());

  const char *SOPClass = 0;
  if (scalarType == VTK_UNSIGNED_CHAR)
    {
    if (numComponents < 3)
      {
      SOPClass = "1.2.840.10008.5.1.4.1.1.7.2";
      }
    else
      {
      SOPClass = "1.2.840.10008.5.1.4.1.1.7.4";
      }
    }
  else if (scalarType == VTK_UNSIGNED_SHORT)
    {
    SOPClass = "1.2.840.10008.5.1.4.1.1.7.3";
    }
  else
    {
    vtkErrorMacro("DICOM Multi-Frame Secondary Capture files require "
                  "unsigned char or unsigned short data");
    return false;
    }

  if (!this->GenerateSOPCommonModule(meta, SOPClass) ||
      !this->GenerateImagePixelModule(meta, info) ||
      !this->GenerateGeneralImageModule(meta) ||
      !this->GenerateGeneralSeriesModule(meta) ||
      !this->GenerateGeneralStudyModule(meta) ||
      !this->GeneratePatientModule(meta) ||
      !this->GenerateSCImageModule(meta) ||
      !this->GenerateSCEquipmentModule(meta))
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateSCMultiFrameInstance(
  vtkInformation *info, vtkDICOMMetaData *meta)
{
  const char *SOPClass = "1.2.840.10008.5.1.4.1.1.3";

  if (!this->GenerateSOPCommonModule(meta, SOPClass) ||
      !this->GenerateImagePixelModule(meta, info) ||
      !this->GenerateGeneralImageModule(meta) ||
      !this->GenerateGeneralSeriesModule(meta) ||
      !this->GenerateGeneralStudyModule(meta) ||
      !this->GeneratePatientModule(meta) ||
      !this->GenerateSCMultiFrameImageModule(meta, info) ||
      !this->GenerateSCEquipmentModule(meta))
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkDICOMSCGenerator::GenerateInstance(
  vtkInformation *info, vtkDICOMMetaData *meta)
{
  if (this->MultiFrame == 0)
    {
    return this->GenerateSCInstance(info, meta);
    }

  return this->GenerateSCMultiFrameInstance(info, meta);
}