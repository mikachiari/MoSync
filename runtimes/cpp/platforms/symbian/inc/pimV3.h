/* Copyright 2013 David Axmark

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef PIMIMPL_H
#error "Bad include!"
#endif	//PIMIMPL_H

//This file is designed to be included in the private section of class Syscall.

void pimInit();
void pimClose();
PimItem* pimGetItem(MAHandle h);

MAHandle maPimListOpen(int listType);
MAHandle maPimListNext(MAHandle list);
int maPimListClose(MAHandle list);

int maPimItemCount(MAHandle item);
int maPimItemGetField(MAHandle item, int index);
int maPimItemFieldCount(MAHandle item, int field);
int maPimItemGetAttributes(MAHandle item, int field, int index);

int maPimFieldType(MAHandle list, int field);

int maPimItemGetValue(MA_PIM_ARGS* args, int index);
int maPimItemSetValue(MA_PIM_ARGS* args, int index, int attributes);
int maPimItemAddValue(MA_PIM_ARGS* args, int attributes);
int maPimItemRemoveValue(MAHandle item, int field, int index);
int maPimItemClose(MAHandle item);

MAHandle maPimItemCreate(MAHandle list);
int maPimItemRemove(MAHandle list, MAHandle item);

HashMap<PimList> mPimLists;
int mPimListNextHandle;
HashMap<Base::PimItem> mPimItems;
int mPimItemNextHandle;
