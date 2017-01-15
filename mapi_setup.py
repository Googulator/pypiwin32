# Use the bottom line as a reference for placement

WinExt_win32com('mapi', libraries="mapi32", pch_header="PythonCOM.h",
                    sources=("""
                        %(mapi)s/mapi.i                 %(mapi)s/mapi.cpp
                        %(mapi)s/PyIABContainer.i       %(mapi)s/PyIABContainer.cpp
                        %(mapi)s/PyIAddrBook.i          %(mapi)s/PyIAddrBook.cpp
                        %(mapi)s/PyIAttach.i            %(mapi)s/PyIAttach.cpp
                        %(mapi)s/PyIDistList.i          %(mapi)s/PyIDistList.cpp
                        %(mapi)s/PyIMailUser.i          %(mapi)s/PyIMailUser.cpp
                        %(mapi)s/PyIMAPIContainer.i     %(mapi)s/PyIMAPIContainer.cpp
                        %(mapi)s/PyIMAPIFolder.i        %(mapi)s/PyIMAPIFolder.cpp
                        %(mapi)s/PyIMAPIProp.i          %(mapi)s/PyIMAPIProp.cpp
                        %(mapi)s/PyIMAPISession.i       %(mapi)s/PyIMAPISession.cpp
                        %(mapi)s/PyIMAPIStatus.i        %(mapi)s/PyIMAPIStatus.cpp
                        %(mapi)s/PyIMAPITable.i         %(mapi)s/PyIMAPITable.cpp
                        %(mapi)s/PyIMessage.i           %(mapi)s/PyIMessage.cpp
                        %(mapi)s/PyIMsgServiceAdmin.i   %(mapi)s/PyIMsgServiceAdmin.cpp
                        %(mapi)s/PyIMsgStore.i          %(mapi)s/PyIMsgStore.cpp
                        %(mapi)s/PyIProfAdmin.i         %(mapi)s/PyIProfAdmin.cpp
                        %(mapi)s/PyIProfSect.i          %(mapi)s/PyIProfSect.cpp
                        %(mapi)s/PyIConverterSession.i	%(mapi)s/PyIConverterSession.cpp
                        %(mapi)s/PyIMAPIAdviseSink.cpp
                        %(mapi)s/mapiutil.cpp
                        %(mapi)s/mapiguids.cpp
                        """ % dirs).split()),
    WinExt_win32com_mapi('exchange', libraries="mapi32",
                         sources=("""
                                  %(mapi)s/exchange.i         %(mapi)s/exchange.cpp
                                  %(mapi)s/PyIExchangeManageStore.i %(mapi)s/PyIExchangeManageStore.cpp
                                  """ % dirs).split()),
    WinExt_win32com_mapi('exchdapi', libraries="mapi32",
                         sources=("""
                                  %(mapi)s/exchdapi.i         %(mapi)s/exchdapi.cpp
                                  """ % dirs).split()),


WinExt_win32com('shell', libraries='shell32', pch_header="shell_pch.h",
