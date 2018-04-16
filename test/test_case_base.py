from test_framework.test_framework import BitcoinTestFramework

class TestCaseBase(BitcoinTestFramework) :
    def set_test_params(self) :
        pass

    def run_test(self) :
        key_list = dir(self)
        
        for name in key_list :
            if name.startswith("initialize") :
                print('Initialize test case:', self.__class__.__name__ + '.' + name)
                getattr(self, name)()

        for name in key_list :
            if name.startswith("test_") :
                print('Test case:', self.__class__.__name__ + '.' + name)
                getattr(self, name)()
        
        for name in key_list :
            if name.startswith("finalize") :
                print('Finalize test case:', self.__class__.__name__ + '.' + name)
                getattr(self, name)()
