
#ifndef PREVIEW_COLUMN_H
#define PREVIEW_COLUMN_H


#include <ColumnTypes.h>

#include <String.h>
#include <Font.h>

class PreviewField : public BStringField
{
public:
								PreviewField(const char* string);
			void 				SetError(bool error);
			bool				Error() const;
private:
			bool 				fError;
};

class PreviewColumn : public BStringColumn
{
public:
								PreviewColumn(const char* title, float width,
									float minWidth, float maxWidth, uint32 truncate,
									alignment align = B_ALIGN_LEFT);
	virtual	void				DrawField(BField* field, BRect rect, BView* parent);
	virtual	bool				AcceptsField(const BField* field) const;
};


#endif	// PREVIEW_COLUMN_H
