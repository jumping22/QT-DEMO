#ifndef RECORDINFO_H
#define RECORDINFO_H

#include <QString>

typedef struct
{
    QString name;
    QString concent;
    QString index;
} STR_CONTRAST_INFO;

typedef struct
{
    QString name;
    QString patientID;
    QString checkID;
    QString exam;
    int weight;
    QString brithday;
    int height;
} STR_PATIENT_INFO;

typedef struct
{
    STR_CONTRAST_INFO cm;
    STR_PATIENT_INFO patient;
    QString injector;
    int datetime;
    QString protocolName;
    int method;
} STR_RECORD_INFO;

typedef struct
{
    int dataType;
    STR_RECORD_INFO record;
} STR_CIM_REPORT_INFO;

#endif // RECORDINFO_H
